#ifndef OPENCBDC_TX_SRC_PYTHON_UTIL_H_
#define OPENCBDC_TX_SRC_PYTHON_UTIL_H_

#include "parsec/util.hpp"

namespace cbdc::parsec::pyutils {
    /// Parse a Python function into the format expected by the Python runner.
    /// \todo This function does not have to be so bespoke
    /// \param filename Name of the Python file containing the function
    /// \param contract_formatter Location of the script used to format the function as expected
    /// \param funcname Name of the function to parse
    auto formContract(const std::string& filename,
                      const std::string& contract_formatter,
                      const std::string& funcname) -> std::string;
}

#endif
