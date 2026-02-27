/*
 * Copyright (c) 2024 EKA2L1 Team.
 *
 * This file is part of EKA2L1 project.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <sdl2/emu_window_sdl2.h>

#include <common/algorithm.h>
#include <common/arghandler.h>
#include <common/cvt.h>
#include <common/fileutils.h>
#include <common/log.h>
#include <common/path.h>
#include <common/pystr.h>
#include <common/sync.h>
#include <common/thread.h>
#include <common/version.h>

#include <config/app_settings.h>
#include <config/config.h>

#include <drivers/audio/audio.h>
#include <drivers/graphics/graphics.h>
#include <drivers/input/common.h>
#include <drivers/input/emu_controller.h>
#include <drivers/sensor/sensor.h>

#include <kernel/kernel.h>
#include <kernel/libmanager.h>

#include <package/manager.h>
#include <services/applist/applist.h>
#include <services/init.h>
#include <services/window/window.h>

#include <system/devices.h>
#include <system/epoc.h>

#include <utils/apacmd.h>
#include <vfs/vfs.h>

#include <gdbstub/gdbstub.h>

#include <atomic>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>

#include <SDL2/SDL.h>

namespace eka2l1::sdl {
    struct emulator_state {
        std::unique_ptr<system> symsys;
        std::unique_ptr<drivers::graphics_driver> graphics_driver;
        std::unique_ptr<drivers::audio_driver> audio_driver;
        std::unique_ptr<drivers::sensor_driver> sensor_driver;
        std::unique_ptr<config::app_settings> app_settings;
        std::unique_ptr<emu_window_sdl2> window;
        drivers::emu_controller_ptr joystick_controller;

        std::atomic<bool> should_emu_quit{false};
        std::atomic<bool> should_emu_pause{false};
        std::atomic<bool> stage_two_inited{false};

        bool app_launch_from_command_line = false;

        common::event graphics_event;
        common::event init_event;
        common::event pause_event;
        common::event kill_event;

        config::state conf;
        window_server *winserv = nullptr;
        std::mutex lockdown;
        std::size_t sys_reset_cbh = 0;

        void stage_one();
        bool stage_two();
        void on_system_reset(system *the_sys);
    };

    void emulator_state::stage_one() {
        log::setup_log(nullptr);
        log::toggle_console();

        conf.deserialize();
        if (log::filterings) {
            log::filterings->parse_filter_string(conf.log_filter);
        }

        LOG_INFO(FRONTEND_CMDLINE, "EKA2L1 SDL2 frontend v0.0.1 ({}-{})", GIT_BRANCH, GIT_COMMIT_HASH);
        app_settings = std::make_unique<config::app_settings>(&conf);

        system_create_components comp;
        comp.audio_ = nullptr;
        comp.graphics_ = nullptr;
        comp.conf_ = &conf;
        comp.settings_ = app_settings.get();

        symsys = std::make_unique<eka2l1::system>(comp);

        device_manager *dvcmngr = symsys->get_device_manager();

        if (dvcmngr->total() > 0) {
            symsys->startup();

            if (conf.enable_gdbstub) {
                symsys->get_gdb_stub()->set_server_port(conf.gdb_port);
            }

            if (!symsys->set_device(conf.device)) {
                LOG_ERROR(FRONTEND_CMDLINE, "Failed to set device index {}, falling back to 0", conf.device);
                conf.device = 0;
                symsys->set_device(0);
            }

            symsys->mount(drive_c, drive_media::physical, add_path(conf.storage, "/drives/c/"), io_attrib_internal);
            symsys->mount(drive_d, drive_media::physical, add_path(conf.storage, "/drives/d/"), io_attrib_internal);
            symsys->mount(drive_e, drive_media::physical, add_path(conf.storage, "/drives/e/"), io_attrib_removeable);

            on_system_reset(symsys.get());
        }

        sys_reset_cbh = symsys->add_system_reset_callback([this](system *the_sys) {
            on_system_reset(the_sys);
        });

        stage_two_inited = false;
    }

    bool emulator_state::stage_two() {
        if (!stage_two_inited) {
            device_manager *dvcmngr = symsys->get_device_manager();
            device *dvc = dvcmngr->get_current();

            if (!dvc) {
                LOG_ERROR(FRONTEND_CMDLINE, "No current device available. Stage two aborted.");
                return false;
            }

            LOG_INFO(FRONTEND_CMDLINE, "Device: {} ({})", dvc->model, dvc->firmware_code);

            symsys->mount(drive_z, drive_media::rom,
                add_path(conf.storage, "/drives/z/"), io_attrib_internal | io_attrib_write_protected);

            drivers::player_type player_be = drivers::player_type_tsf;
            if (conf.midi_backend == config::MIDI_BACKEND_MINIBAE)
                player_be = drivers::player_type_minibae;

            audio_driver = drivers::make_audio_driver(drivers::audio_driver_backend::cubeb,
                conf.audio_master_volume, player_be);

            if (audio_driver) {
                audio_driver->set_bank_path(drivers::MIDI_BANK_TYPE_HSB, conf.hsb_bank_path);
                audio_driver->set_bank_path(drivers::MIDI_BANK_TYPE_SF2, conf.sf2_bank_path);
            }

            symsys->set_audio_driver(audio_driver.get());

            sensor_driver = drivers::sensor_driver::instantiate();
            symsys->set_sensor_driver(sensor_driver.get());
            symsys->initialize_user_parties();

            if (!conf.svg_icon_cache_reset) {
                common::delete_folder("cache\\");
                conf.svg_icon_cache_reset = true;
                conf.serialize(false);
            }

            std::vector<std::tuple<std::u16string, std::string, epocver>> dlls_need_to_copy = {
                { u"Z:\\sys\\bin\\goommonitor.dll", "patch\\goommonitor_general.dll", epocver::epoc94 },
                { u"Z:\\sys\\bin\\avkonfep.dll", "patch\\avkonfep_general.dll", epocver::epoc93fp1 }
            };

            io_system *io = symsys->get_io_system();

            for (auto &[org_path, patch_path, ver_required] : dlls_need_to_copy) {
                if (symsys->get_symbian_version_use() < ver_required)
                    continue;

                auto where_to_copy = io->get_raw_path(org_path);
                if (where_to_copy.has_value()) {
                    std::string dest = common::ucs2_to_utf8(where_to_copy.value());
                    std::string backup = dest + ".bak";
                    if (common::exists(dest) && !common::exists(backup))
                        common::move_file(dest, backup);
                    common::copy_file(patch_path, dest, true);
                }
            }

            manager::packages *pkgmngr = symsys->get_packages();
            pkgmngr->load_registries();
            pkgmngr->migrate_legacy_registries();

            stage_two_inited = true;
        }

        return true;
    }

    void emulator_state::on_system_reset(system *the_sys) {
        winserv = reinterpret_cast<window_server *>(the_sys->get_kernel_system()->get_by_name<service::server>(
            get_winserv_name_by_epocver(symsys->get_symbian_version_use())));

        if (stage_two_inited) {
            symsys->initialize_user_parties();
        }
    }

    // Input helpers

    static drivers::input_event make_mouse_event_driver(float x, float y, float z, int button, int action, int mouse_id) {
        drivers::input_event evt;
        evt.type_ = drivers::input_event_type::touch;
        evt.mouse_.raw_screen_pos_ = false;
        evt.mouse_.pos_x_ = static_cast<int>(x);
        evt.mouse_.pos_y_ = static_cast<int>(y);
        evt.mouse_.pos_z_ = static_cast<int>(z);
        evt.mouse_.mouse_id = static_cast<std::uint32_t>(mouse_id);
        evt.mouse_.button_ = static_cast<drivers::mouse_button>(button);
        evt.mouse_.action_ = static_cast<drivers::mouse_action>(action);
        return evt;
    }

    static drivers::input_event make_key_event_driver(int key, drivers::key_state state) {
        drivers::input_event evt;
        evt.type_ = drivers::input_event_type::key;
        evt.key_.state_ = state;
        evt.key_.code_ = key;
        return evt;
    }

    void on_mouse_evt(void *userdata, vec3 pos, int button, int action, int mouse_id) {
        auto *emu = reinterpret_cast<emulator_state *>(userdata);
        const float scale = emu->symsys->get_config()->ui_scale;
        auto evt = make_mouse_event_driver(
            static_cast<float>(pos.x) / scale,
            static_cast<float>(pos.y) / scale,
            static_cast<float>(pos.z) / scale,
            button, action, mouse_id);

        const std::lock_guard<std::mutex> guard(emu->lockdown);
        if (emu->winserv)
            emu->winserv->queue_input_from_driver(evt);
    }

    void on_key_press(void *userdata, std::uint32_t key) {
        auto *emu = reinterpret_cast<emulator_state *>(userdata);
        auto evt = make_key_event_driver(static_cast<int>(key), drivers::key_state::pressed);

        const std::lock_guard<std::mutex> guard(emu->lockdown);
        if (emu->winserv)
            emu->winserv->queue_input_from_driver(evt);
    }

    void on_key_release(void *userdata, std::uint32_t key) {
        auto *emu = reinterpret_cast<emulator_state *>(userdata);
        auto evt = make_key_event_driver(static_cast<int>(key), drivers::key_state::released);

        const std::lock_guard<std::mutex> guard(emu->lockdown);
        if (emu->winserv)
            emu->winserv->queue_input_from_driver(evt);
    }

    // Thread functions

    static void graphics_driver_thread(emulator_state &state) {
        common::set_thread_name("Graphics thread");
        common::set_thread_priority(common::thread_priority_high);

        state.graphics_driver = drivers::create_graphics_driver(
            drivers::graphic_api::opengl, state.window->get_window_system_info());
        state.graphics_driver->update_surface_size(state.window->window_fb_size());

        state.window->resize_hook = [](void *userdata, const vec2 &size) {
            auto *s = reinterpret_cast<emulator_state *>(userdata);
            s->graphics_driver->update_surface_size(size);
        };

        state.symsys->set_graphics_driver(state.graphics_driver.get());

        drivers::emu_window *win = state.window.get();
        state.graphics_driver->set_display_hook([win]() {
            win->swap_buffer();
        });

        state.joystick_controller = drivers::new_emu_controller(drivers::controller_type::sdl2);
        state.joystick_controller->start_polling();

        state.graphics_event.set();
        state.graphics_driver->run();

        if (state.stage_two_inited)
            state.graphics_event.wait();

        state.joystick_controller->stop_polling();
        state.graphics_driver.reset();
    }

    static void os_thread(emulator_state &state) {
        common::set_thread_name("Symbian OS thread");
        common::set_thread_priority(common::thread_priority_high);

        bool first_time = true;

        while (true) {
            if (state.should_emu_quit)
                break;

            const bool success = state.stage_two();
            state.init_event.set();

            if (first_time) {
                state.graphics_event.wait();
                first_time = false;
            }

            if (success)
                break;

            state.init_event.reset();
            state.init_event.wait();
        }

        while (!state.should_emu_quit) {
            state.symsys->loop();

            if (state.should_emu_pause && !state.should_emu_quit) {
                state.pause_event.wait();
                state.pause_event.reset();
            }
        }

        state.kill_event.wait();
        state.symsys.reset();
        state.graphics_event.set();
    }

    void kill_emulator(emulator_state &state) {
        state.should_emu_quit = true;
        state.should_emu_pause = false;
        state.pause_event.set();

        kernel_system *kern = state.symsys ? state.symsys->get_kernel_system() : nullptr;
        if (kern)
            kern->stop_cores_idling();

        if (state.graphics_driver)
            state.graphics_driver->abort();

        state.init_event.set();
        state.kill_event.set();
    }

    // CLI handlers

    static bool help_handler(common::arg_parser *parser, void *, std::string *) {
        std::cout << parser->get_help_string();
        return false;
    }

    static bool list_devices_handler(common::arg_parser *, void *userdata, std::string *) {
        auto *emu = reinterpret_cast<emulator_state *>(userdata);
        auto &devices = emu->symsys->get_device_manager()->get_devices();

        for (std::size_t i = 0; i < devices.size(); i++) {
            std::cout << i << " : " << devices[i].model << " (" << devices[i].firmware_code << ")" << std::endl;
        }
        return false;
    }

    static bool list_apps_handler(common::arg_parser *, void *userdata, std::string *err) {
        auto *emu = reinterpret_cast<emulator_state *>(userdata);
        kernel_system *kern = emu->symsys->get_kernel_system();

        applist_server *svr = kern ? reinterpret_cast<applist_server *>(
            kern->get_by_name<service::server>(
                get_app_list_server_name_by_epocver(kern->get_epoc_version()))) : nullptr;

        if (!svr) {
            *err = "Can't get app list server!";
            return false;
        }

        auto &regs = svr->get_registerations();
        for (std::size_t i = 0; i < regs.size(); i++) {
            std::string name = common::ucs2_to_utf8(regs[i].mandatory_info.long_caption.to_std_string(nullptr));
            std::cout << i << " : " << name << " (UID: 0x" << std::hex << regs[i].mandatory_info.uid << std::dec << ")" << std::endl;
        }

        return false;
    }

    static bool app_run_handler(common::arg_parser *parser, void *userdata, std::string *err) {
        const char *tok = parser->next_token();
        if (!tok) {
            *err = "No application specified";
            return false;
        }

        std::string tokstr = tok;
        const char *cmdline_peek = parser->peek_token();
        std::string cmdlinestr;

        if (cmdline_peek) {
            cmdlinestr = cmdline_peek;
            if (cmdlinestr.substr(0, 2) == "--") {
                cmdlinestr.clear();
            } else {
                parser->next_token();
            }
        }

        auto *emu = reinterpret_cast<emulator_state *>(userdata);
        kernel_system *kern = emu->symsys->get_kernel_system();

        applist_server *svr = kern ? reinterpret_cast<applist_server *>(
            kern->get_by_name<service::server>(
                get_app_list_server_name_by_epocver(kern->get_epoc_version()))) : nullptr;

        if (!svr) {
            *err = "Can't get app list server!";
            return false;
        }

        // UID-based launch
        if (tokstr.length() > 2 && tokstr.substr(0, 2) == "0x") {
            std::uint32_t uid = common::pystr(tokstr).as_int<std::uint32_t>();
            apa_app_registry *registry = svr->get_registration(uid);

            if (registry) {
                epoc::apa::command_line cmd;
                cmd.launch_cmd_ = epoc::apa::command_create;
                svr->launch_app(*registry, cmd, nullptr, nullptr);
                emu->app_launch_from_command_line = true;
                return true;
            }

            *err = "App with UID " + tokstr + " not found";
            return false;
        }

        // Path-based launch
        if (has_root_dir(tokstr)) {
            process_ptr pr = kern->spawn_new_process(common::utf8_to_ucs2(tokstr), common::utf8_to_ucs2(cmdlinestr));
            if (!pr) {
                *err = "Unable to launch process: " + tokstr;
                return false;
            }
            pr->run();
            emu->app_launch_from_command_line = true;
            return true;
        }

        // Name-based launch
        auto &regs = svr->get_registerations();
        for (auto &reg : regs) {
            if (common::ucs2_to_utf8(reg.mandatory_info.long_caption.to_std_string(nullptr)) == tokstr) {
                epoc::apa::command_line cmd;
                cmd.launch_cmd_ = epoc::apa::command_create;
                svr->launch_app(reg, cmd, nullptr, nullptr);
                emu->app_launch_from_command_line = true;
                return true;
            }
        }

        *err = "No app found with name: " + tokstr;
        return false;
    }

    static bool device_set_handler(common::arg_parser *parser, void *userdata, std::string *err) {
        const char *device = parser->next_token();
        if (!device) {
            *err = "No device specified";
            return false;
        }

        auto *emu = reinterpret_cast<emulator_state *>(userdata);
        auto &devices = emu->symsys->get_device_manager()->get_devices();

        for (std::size_t i = 0; i < devices.size(); i++) {
            if (device == devices[i].firmware_code) {
                if (emu->conf.device != static_cast<int>(i)) {
                    emu->conf.device = static_cast<int>(i);
                    emu->symsys->set_device(static_cast<std::uint8_t>(i));
                }
                return true;
            }
        }

        *err = "Device not found: " + std::string(device);
        return false;
    }

    static bool install_handler(common::arg_parser *parser, void *userdata, std::string *err) {
        const char *path = parser->next_token();
        if (!path) {
            *err = "No SIS path given";
            return false;
        }

        auto *emu = reinterpret_cast<emulator_state *>(userdata);
        if (!emu->symsys->install_package(common::utf8_to_ucs2(path), drive_e)) {
            *err = "SIS installation failed";
            return false;
        }
        return true;
    }

    static bool mount_card_handler(common::arg_parser *parser, void *userdata, std::string *err) {
        auto *emu = reinterpret_cast<emulator_state *>(userdata);
        const char *path = parser->next_token();
        if (!path) {
            *err = "No folder specified";
            return true;
        }

        io_system *io = emu->symsys->get_io_system();
        io->unmount(drive_e);

        if (common::is_dir(path)) {
            io->mount_physical_path(drive_e, drive_media::physical,
                io_attrib_removeable | io_attrib_write_protected,
                common::utf8_to_ucs2(path));
        } else {
            std::cout << "Mounting ZIP, please wait..." << std::endl;
            auto error = emu->symsys->mount_game_zip(drive_e, drive_media::physical, path, io_attrib_write_protected);
            if (error != zip_mount_error_none) {
                *err = "ZIP mount failed";
                return false;
            }
        }
        return true;
    }

}  // namespace eka2l1::sdl

int main(int argc, char *argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    eka2l1::sdl::emulator_state state;
    state.stage_one();

    std::thread os_thread_obj(eka2l1::sdl::os_thread, std::ref(state));
    state.init_event.wait();

    eka2l1::common::arg_parser parser(argc, const_cast<const char **>(argv));

    parser.add("--help, -h", "Display help", eka2l1::sdl::help_handler);
    parser.add("--listapp", "List installed applications", eka2l1::sdl::list_apps_handler);
    parser.add("--listdevices", "List installed devices", eka2l1::sdl::list_devices_handler);
    parser.add("--app, -a, --run", "Run an app by name, UID (0x...), or virtual path", eka2l1::sdl::app_run_handler);
    parser.add("--device, -dvc", "Set device by firmware code", eka2l1::sdl::device_set_handler);
    parser.add("--install, -i", "Install a SIS package", eka2l1::sdl::install_handler);
    parser.add("--mount, -m", "Mount a folder/zip as Game Card ROM on E:", eka2l1::sdl::mount_card_handler);

    if (argc > 1) {
        std::string err;
        state.should_emu_quit = !parser.parse(&state, &err);

        if (state.should_emu_quit) {
            state.graphics_event.set();
            state.kill_event.set();

            if (!err.empty())
                std::cerr << err << std::endl;

            os_thread_obj.join();
            SDL_Quit();
            return err.empty() ? 0 : 1;
        }
    }

    state.window = std::make_unique<eka2l1::sdl::emu_window_sdl2>();

    state.window->raw_mouse_event = eka2l1::sdl::on_mouse_evt;
    state.window->button_pressed = eka2l1::sdl::on_key_press;
    state.window->button_released = eka2l1::sdl::on_key_release;

    state.window->init("EKA2L1", eka2l1::vec2(800, 600), eka2l1::drivers::emu_window_flag_maximum_size);
    state.window->set_userdata(&state);
    state.window->close_hook = [](void *userdata) {
        auto *s = reinterpret_cast<eka2l1::sdl::emulator_state *>(userdata);
        eka2l1::sdl::kill_emulator(*s);
    };

    std::thread graphics_thread_obj(eka2l1::sdl::graphics_driver_thread, std::ref(state));

    // Main thread runs the SDL event loop
    while (!state.should_emu_quit && !state.window->should_quit()) {
        state.window->poll_events();
        SDL_Delay(1);
    }

    if (!state.should_emu_quit)
        eka2l1::sdl::kill_emulator(state);

    graphics_thread_obj.join();
    os_thread_obj.join();

    SDL_Quit();
    return 0;
}
