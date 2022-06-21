// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
//                    MITRE Corporation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "config/tools/config_generator.hpp"
#include "util/common/config.hpp"

#include <gtest/gtest.h>
#include <string>

// TODO: Add file parsing tests.

class config_generation_validation_test : public ::testing::Test {
  protected:
    void SetUp() override {
        template_file_atomizer
            = "../config/unit/atomizer_template_unit_test.tmpl";
        template_file_2pc = "../config/unit/2pc_template_unit_test.tmpl";
        port_num = 5555;
    }

    std::string template_file_atomizer;
    std::string template_file_2pc;
    size_t port_num;
};

TEST_F(config_generation_validation_test,
       generate_configuration_file_atomizer_test) {
    cbdc::generate_config::config_generator new_config_gen(
        template_file_atomizer,
        port_num);
    auto cfg_or_err = new_config_gen.generate_configuration_file();
    ASSERT_EQ(cfg_or_err, "SUCCESS");
    // TODO
    // Reload generate file and check values
    // Delete generated file
}

TEST_F(config_generation_validation_test,
       generate_configuration_file_two_phase_test) {
    cbdc::generate_config::config_generator new_config_gen(template_file_2pc,
                                                           port_num);
    auto cfg_or_err = new_config_gen.generate_configuration_file();
    ASSERT_EQ(cfg_or_err, "SUCCESS");
    // TODO
    // Reload generate file and check values
    // Delete generated file
}
