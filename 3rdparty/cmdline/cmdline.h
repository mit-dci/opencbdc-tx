/*
  Copyright (c) 2009, Hideyuki Tanaka
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
  * Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
  * Neither the name of the <organization> nor the
  names of its contributors may be used to endorse or promote products
  derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY <copyright holder> ''AS IS'' AND ANY
  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL <copyright holder> BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  Modified by MIT Digital Currency Initiative, 2021.
*/

#ifndef CMDLINE_H
#define CMDLINE_H

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <vector>

namespace cmdline {
    namespace detail {
        template<typename Target, typename Source, bool Same>
        class lexical_cast_t {
          public:
            static std::optional<Target> cast(const Source& arg) {
                Target ret;
                std::stringstream ss;
                if(!(ss << arg && ss >> ret && ss.eof())) {
                    return std::nullopt;
                }
                return ret;
            }
        };

        template<typename Target, typename Source>
        class lexical_cast_t<Target, Source, true> {
          public:
            static Target cast(const Source& arg) {
                return arg;
            }
        };

        template<typename Source>
        class lexical_cast_t<std::string, Source, false> {
          public:
            static std::string cast(const Source& arg) {
                std::ostringstream ss;
                ss << arg;
                return ss.str();
            }
        };

        template<typename Target>
        class lexical_cast_t<Target, std::string, false> {
          public:
            static Target cast(const std::string& arg) {
                Target ret;
                std::istringstream ss(arg);
                ss >> ret;
                assert(ss.eof());
                return ret;
            }
        };

        template<typename Target, typename Source>
        Target lexical_cast(const Source& arg) {
            return lexical_cast_t<Target,
                                  Source,
                                  std::is_same_v<Target, Source>>::cast(arg);
        }

        template<class T>
        std::string readable_typename() {
            return typeid(T).name();
        }

        template<class T>
        std::string default_value(T def) {
            return detail::lexical_cast<std::string>(def);
        }

        template<>
        inline std::string readable_typename<std::string>() {
            return "string";
        }

    } // detail

    //-----

    template<class T>
    struct default_reader {
        T operator()(const std::string& str) {
            return detail::lexical_cast<T>(str);
        }
    };

    template<class T>
    struct range_reader {
        range_reader(const T& low, const T& high) : m_low(low), m_high(high) {}
        std::optional<T> operator()(const std::string& s) const {
            T ret = default_reader<T>()(s);
            if(!(ret >= m_low && ret <= m_high)) {
                return std::nullopt;
            }
            return ret;
        }

      private:
        T m_low, m_high;
    };

    template<class T>
    range_reader<T> range(const T& low, const T& high) {
        return range_reader<T>(low, high);
    }

    template<class T>
    struct oneof_reader {
        std::optional<T> operator()(const std::string& s) {
            T ret = default_reader<T>()(s);
            if(std::find(m_alt.begin(), m_alt.end(), ret) == m_alt.end()) {
                return std::nullopt;
            }
            return ret;
        }

        template<typename... Ta>
        void add(const Ta&... vs) {
            ((m_alt.push_back(vs), ...));
        }

      private:
        std::vector<T> m_alt;
    };

    template<class T, typename... Ta>
    oneof_reader<T> oneof(Ta... vs) {
        oneof_reader<T> ret;
        ret.add(vs...);
        return ret;
    }

    //-----

    class parser {
      public:
        parser() {}
        ~parser() {}

        void add(const std::string& name,
                 char short_name = 0,
                 const std::string& desc = "") {
            if(m_options.count(name)) {
                m_errors.push_back(name + " already added.");
                return;
            }
            m_options[name]
                = std::make_shared<option_without_value>(name,
                                                         short_name,
                                                         desc);
            m_ordered.push_back(m_options[name]);
        }

        template<class T>
        void add(const std::string& name,
                 char short_name = 0,
                 const std::string& desc = "",
                 bool need = true,
                 const T def = T()) {
            add(name, short_name, desc, need, def, default_reader<T>());
        }

        template<class T, class F>
        void add(const std::string& name,
                 char short_name = 0,
                 const std::string& desc = "",
                 bool need = true,
                 const T def = T(),
                 F reader = F()) {
            if(m_options.count(name)) {
                m_errors.push_back("Duplicate option declaration:" + name);
                return;
            }
            m_options[name]
                = std::make_shared<option_with_value_with_reader<T, F>>(
                    name,
                    short_name,
                    need,
                    def,
                    desc,
                    reader);
            m_ordered.push_back(m_options[name]);
        }

        void footer(const std::string& f) {
            m_ftr = f;
        }

        void set_program_name(const std::string& name) {
            m_prog_name = name;
        }

        bool exist(const std::string& name) const {
            return (m_options.count(name) != 0)
                && m_options.find(name)->second->has_set();
        }

        template<class T>
        std::optional<T> get(const std::string& name) const {
            if(auto it = m_options.find(name); it != m_options.end()) {
                auto p = std::dynamic_pointer_cast<option_with_value<T>>(
                    it->second);
                return p->get();
            }
            return std::nullopt;
        }

        const std::vector<std::string>& rest() const {
            return m_others;
        }

        bool parse(const std::string& arg) {
            std::vector<std::string> args;

            std::string buf;
            bool in_quote = false;
            for(std::string::size_type i = 0; i < arg.length(); i++) {
                if(arg[i] == '\"') {
                    in_quote = !in_quote;
                    continue;
                }

                if(arg[i] == ' ' && !in_quote) {
                    args.push_back(buf);
                    buf = "";
                    continue;
                }

                if(arg[i] == '\\') {
                    i++;
                    if(i >= arg.length()) {
                        m_errors.push_back(
                            "unexpected occurrence of '\\' at end of string");
                        return false;
                    }
                }

                buf += arg[i];
            }

            if(in_quote) {
                m_errors.push_back("quote is not closed");
                return false;
            }

            if(buf.length() > 0)
                args.push_back(buf);

            for(size_t i = 0; i < args.size(); i++)
                std::cout << "\"" << args[i] << "\"" << std::endl;

            return parse(args);
        }

        bool parse(const std::vector<std::string>& args) {
            int argc = static_cast<int>(args.size());
            std::vector<const char*> argv(argc);

            for(int i = 0; i < argc; i++)
                argv[i] = args[i].c_str();

            return parse(argc, &argv[0]);
        }

        bool parse(int argc, const char* const argv[]) {
            m_others.clear();

            if(argc < 1) {
                m_errors.push_back("argument number must be longer than 0");
                return false;
            }
            if(m_prog_name == "")
                m_prog_name = argv[0];

            std::map<char, std::string> lookup;
            for(std::map<std::string, std::shared_ptr<option_base>>::iterator p
                = m_options.begin();
                p != m_options.end();
                p++) {
                if(p->first.length() == 0)
                    continue;
                char initial = p->second->short_name();
                if(initial) {
                    if(lookup.count(initial) > 0) {
                        lookup[initial] = "";
                        m_errors.push_back(std::string("short option '")
                                           + initial + "' is ambiguous");
                        return false;
                    } else
                        lookup[initial] = p->first;
                }
            }

            for(int i = 1; i < argc; i++) {
                if(strncmp(argv[i], "--", 2) == 0) {
                    const char* p = strchr(argv[i] + 2, '=');
                    if(p) {
                        std::string name(argv[i] + 2, p);
                        std::string val(p + 1);
                        set_option(name, val);
                    } else {
                        std::string name(argv[i] + 2);
                        if(m_options.count(name) == 0) {
                            m_errors.push_back("undefined option: --" + name);
                            continue;
                        }
                        if(m_options[name]->has_value()) {
                            if(i + 1 >= argc) {
                                m_errors.push_back("option needs value: --"
                                                   + name);
                                continue;
                            } else {
                                i++;
                                set_option(name, argv[i]);
                            }
                        } else {
                            set_option(name);
                        }
                    }
                } else if(strncmp(argv[i], "-", 1) == 0) {
                    if(!argv[i][1])
                        continue;
                    char last = argv[i][1];
                    for(int j = 2; argv[i][j]; j++) {
                        last = argv[i][j];
                        if(lookup.count(argv[i][j - 1]) == 0) {
                            m_errors.push_back(
                                std::string("undefined short option: -")
                                + argv[i][j - 1]);
                            continue;
                        }
                        if(lookup[argv[i][j - 1]] == "") {
                            m_errors.push_back(
                                std::string("ambiguous short option: -")
                                + argv[i][j - 1]);
                            continue;
                        }
                        set_option(lookup[argv[i][j - 1]]);
                    }

                    if(lookup.count(last) == 0) {
                        m_errors.push_back(
                            std::string("undefined short option: -") + last);
                        continue;
                    }
                    if(lookup[last] == "") {
                        m_errors.push_back(
                            std::string("ambiguous short option: -") + last);
                        continue;
                    }

                    if(i + 1 < argc && m_options[lookup[last]]->has_value()) {
                        set_option(lookup[last], argv[i + 1]);
                        i++;
                    } else {
                        set_option(lookup[last]);
                    }
                } else {
                    m_others.push_back(argv[i]);
                }
            }

            for(std::map<std::string, std::shared_ptr<option_base>>::iterator p
                = m_options.begin();
                p != m_options.end();
                p++)
                if(!p->second->valid())
                    m_errors.push_back("need option: --"
                                       + std::string(p->first));

            return m_errors.size() == 0;
        }

        void parse_check(const std::string& arg) {
            if(!m_options.count("help"))
                add("help", '?', "print this message");
            check(0, parse(arg));
        }

        void parse_check(const std::vector<std::string>& args) {
            if(!m_options.count("help"))
                add("help", '?', "print this message");
            check(args.size(), parse(args));
        }

        void parse_check(int argc, char* argv[]) {
            if(!m_options.count("help"))
                add("help", '?', "print this message");
            check(argc, parse(argc, argv));
        }

        std::string error() const {
            return m_errors.size() > 0 ? m_errors[0] : "";
        }

        std::string error_full() const {
            std::ostringstream oss;
            for(size_t i = 0; i < m_errors.size(); i++)
                oss << m_errors[i] << std::endl;
            return oss.str();
        }

        std::string usage() const {
            std::ostringstream oss;
            oss << "usage: " << m_prog_name << " ";
            for(size_t i = 0; i < m_ordered.size(); i++) {
                if(m_ordered[i]->must())
                    oss << m_ordered[i]->short_description() << " ";
            }

            oss << "[options] ... " << m_ftr << std::endl;
            oss << "options:" << std::endl;

            size_t max_width = 0;
            for(size_t i = 0; i < m_ordered.size(); i++) {
                max_width = std::max(max_width, m_ordered[i]->name().length());
            }
            for(size_t i = 0; i < m_ordered.size(); i++) {
                if(m_ordered[i]->short_name()) {
                    oss << "  -" << m_ordered[i]->short_name() << ", ";
                } else {
                    oss << "      ";
                }

                oss << "--" << m_ordered[i]->name();
                for(size_t j = m_ordered[i]->name().length();
                    j < max_width + 4;
                    j++)
                    oss << ' ';
                oss << m_ordered[i]->description() << std::endl;
            }
            return oss.str();
        }

      private:
        void check(int argc, bool ok) {
            if((argc == 1 && !ok) || exist("help")) {
                std::cerr << usage();
                exit(0);
            }

            if(!ok) {
                std::cerr << error() << std::endl << usage();
                exit(1);
            }
        }

        void set_option(const std::string& name) {
            if(m_options.count(name) == 0) {
                m_errors.push_back("undefined option: --" + name);
                return;
            }
            if(!m_options[name]->set()) {
                m_errors.push_back("option needs value: --" + name);
                return;
            }
        }

        void set_option(const std::string& name, const std::string& value) {
            if(m_options.count(name) == 0) {
                m_errors.push_back("undefined option: --" + name);
                return;
            }
            if(!m_options[name]->set(value)) {
                m_errors.push_back("option value is invalid: --" + name + "="
                                   + value);
                return;
            }
        }

        class option_base {
          public:
            virtual ~option_base() {}

            virtual bool has_value() const = 0;
            virtual bool set() = 0;
            virtual bool set(const std::string& value) = 0;
            virtual bool has_set() const = 0;
            virtual bool valid() const = 0;
            virtual bool must() const = 0;

            virtual const std::string& name() const = 0;
            virtual char short_name() const = 0;
            virtual const std::string& description() const = 0;
            virtual std::string short_description() const = 0;
        };

        class option_without_value : public option_base {
          public:
            option_without_value(const std::string& name,
                                 char short_name,
                                 const std::string& desc)
                : m_name(name),
                  m_sname(short_name),
                  m_desc(desc),
                  m_has_val(false) {}
            ~option_without_value() {}

            bool has_value() const {
                return false;
            }

            bool set() {
                m_has_val = true;
                return true;
            }

            bool set(const std::string&) {
                return false;
            }

            bool has_set() const {
                return m_has_val;
            }

            bool valid() const {
                return true;
            }

            bool must() const {
                return false;
            }

            const std::string& name() const {
                return m_name;
            }

            char short_name() const {
                return m_sname;
            }

            const std::string& description() const {
                return m_desc;
            }

            std::string short_description() const {
                return "--" + m_name;
            }

          private:
            std::string m_name;
            char m_sname;
            std::string m_desc;
            bool m_has_val;
        };

        template<class T>
        class option_with_value : public option_base {
          public:
            option_with_value(const std::string& name,
                              char short_name,
                              bool need,
                              const T& def,
                              const std::string& desc)
                : m_name(name),
                  m_sname(short_name),
                  m_need(need),
                  m_has_val(false),
                  m_def(def),
                  m_actual(def) {
                this->m_desc = full_description(desc);
            }
            ~option_with_value() {}

            const std::optional<T>& get() const {
                return m_actual;
            }

            bool has_value() const {
                return true;
            }

            bool set() {
                return false;
            }

            bool set(const std::string& value) {
                m_actual = read(value);
                if(!m_actual.has_value()) {
                    return false;
                }
                m_has_val = true;
                return true;
            }

            bool has_set() const {
                return m_has_val;
            }

            bool valid() const {
                if(m_need && !m_has_val)
                    return false;
                return true;
            }

            bool must() const {
                return m_need;
            }

            const std::string& name() const {
                return m_name;
            }

            char short_name() const {
                return m_sname;
            }

            const std::string& description() const {
                return m_desc;
            }

            std::string short_description() const {
                return "--" + m_name + "=" + detail::readable_typename<T>();
            }

          protected:
            std::string full_description(const std::string& full_desc) {
                return full_desc + " (" + detail::readable_typename<T>()
                     + (m_need ? ""
                               : " [=" + detail::default_value<T>(m_def) + "]")
                     + ")";
            }

            virtual std::optional<T> read(const std::string& s) = 0;

            std::string m_name;
            char m_sname;
            bool m_need;
            std::string m_desc;

            bool m_has_val;
            T m_def;
            std::optional<T> m_actual;
        };

        template<class T, class F>
        class option_with_value_with_reader : public option_with_value<T> {
          public:
            option_with_value_with_reader(const std::string& name,
                                          char short_name,
                                          bool need,
                                          const T def,
                                          const std::string& desc,
                                          F reader)
                : option_with_value<T>(name, short_name, need, def, desc),
                  m_reader(reader) {}

          private:
            std::optional<T> read(const std::string& s) {
                return m_reader(s);
            }

            F m_reader;
        };

        std::map<std::string, std::shared_ptr<option_base>> m_options;
        std::vector<std::shared_ptr<option_base>> m_ordered;
        std::string m_ftr;

        std::string m_prog_name;
        std::vector<std::string> m_others;

        std::vector<std::string> m_errors;
    };

}

#endif // CMDLINE_H
