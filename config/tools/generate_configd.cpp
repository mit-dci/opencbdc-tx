#include "config_generator.hpp"

#include <iostream>
#include <random>

auto main(int argc, char** argv) -> int {
    auto args = cbdc::config::get_args(argc, argv);

    std::mt19937 rng;
    static const auto m_random_source
        = std::make_unique<cbdc::random_source>(cbdc::config::random_source);

    if(args.size() < 3) {
        std::cerr << "Usage: " << args[0]
                  << " <config template file> <starting port number to "
                     "increment from>"
                  << std::endl;
        return 0;
    }

    auto port_is_valid = std::isdigit(*args[2].c_str());
    if(!port_is_valid) {
        std::cerr << "Port number provided, " << args[2]
                  << ", is not a valid number. Exiting..." << std::endl;
        return 0;
    }
    size_t port_num = static_cast<size_t>(std::stoull(args[2]));
    if(port_num > MAX_PORT_NUM) {
        std::cerr << "Port number provided, " << args[2]
                  << ", is too large. Exiting..." << std::endl;
        return 0;
    }
    cbdc::generate_config::config_generator new_config_gen(args[1], port_num);
    auto cfg_or_err = new_config_gen.generate_configuration_file();
    std::cout << cfg_or_err << std::endl;
    return 0;
}
