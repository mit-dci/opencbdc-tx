// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
//                    MITRE Corporation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "config_generator.hpp"

#include <iostream>

auto main(int argc, char** argv) -> int {
    auto args = cbdc::config::get_args(argc, argv);
    //  Min and max number of params
    size_t max_param_num = 4;
    size_t min_param_num = 3;
    // Default build dir
    std::string build_dir = "build";
    // Help string
    std::string help_string
        = "Usage: " + args[0]
        + "  <config template file>  <starting port number>  <build "
          "directory> \n\nPARAM 1, <config template file> : The relative "
          "path from current working directory to the template configuration "
          "file including the filename itself.\nPARAM 2, <starting port "
          "number> : The first port number to use and increment from. Must be "
          "less than 65535.\nPARAM 3, <build directory> : The path relative "
          "to project root directory, but not including project root "
          "directory itself, of the build directory. Use '/' as separators if "
          "build dir has depth is greater than 1. E.g. if build directory is "
          "located at '<project root>/tmp/build' input should be 'tmp/build'. "
          "(defaults to 'build' if left empty).";
    auto exit_bool = false;
    if(args.size() < min_param_num || args.size() > max_param_num) {
        // Catch error case where there are no params
        std::cerr << help_string << std::endl
                  << std::endl
                  << "Rerun with proper parameters." << std::endl;
        exit_bool = true;
    } else if(args.size() == 2 && (args[1] == "-h" || args[1] == "--help")) {
        // Catch help in case user intuitively types this
        std::cout << help_string << std::endl;
        exit_bool = true;
    } else if(args.size() == min_param_num) {
        // Case where user does not input a build dir
        std::cout << "No build directory name specified as third. Using "
                     "default name of 'build'"
                  << std::endl;
    } else if(args.size() == max_param_num) {
        // Case where user DOES input build dir
        build_dir = args[max_param_num - 1];
    }

    auto port_is_valid = std::isdigit(*args[2].c_str());
    // Catch error case where port numbers are invalid (either not a number or
    // too large)
    if(port_is_valid == 0) {
        std::cerr << "Port number provided, " << args[2]
                  << ", is not a valid number. Exiting..." << std::endl;
        exit_bool = true;
    }
    auto port_num = static_cast<size_t>(std::stoull(args[2]));
    if(port_num > MAX_PORT_NUM) {
        std::cerr << "Port number provided, " << args[2]
                  << ", is too large. Exiting..." << std::endl;
        exit_bool = true;
    }
    if(exit_bool) {
        return 0;
    }
    cbdc::generate_config::config_generator new_config_gen(args[1],
                                                           port_num,
                                                           build_dir);
    auto cfg_or_err = new_config_gen.generate_configuration_file();
    std::cout << cfg_or_err << std::endl;
    return 0;
}
