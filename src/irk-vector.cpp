// MIT License
//
// Copyright (c) 2018 Michal Siedlaczek
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

//! \file
//! \author     Michal Siedlaczek
//! \copyright  MIT License

#include <cstddef>

#include <CLI/CLI.hpp>
#include <boost/filesystem.hpp>
#include <fmt/format.h>

#include <irkit/memoryview.hpp>
#include <irkit/vector.hpp>
#include "cli.hpp"
#include "run_query.hpp"

using namespace irk::cli;
using ir::Vector_View;

enum class Type_Descriptor : uint8_t {
    i8 = 0,
    i16 = 1,
    i32 = 2,
    i64 = 3,
    u8 = 4,
    u16 = 5,
    u32 = 6,
    u64 = 7,
    f32 = 8,
    f64 = 9,
};
inline std::ostream& operator<<(std::ostream& os, Type_Descriptor descriptor)
{
    switch (descriptor) {
    case Type_Descriptor::i8: os << "i8"; break;
    case Type_Descriptor::i16: os << "i16"; break;
    case Type_Descriptor::i32: os << "i32"; break;
    case Type_Descriptor::i64: os << "i64"; break;
    case Type_Descriptor::u8:  os << "i8"; break;
    case Type_Descriptor::u16: os << "i16"; break;
    case Type_Descriptor::u32: os << "i32"; break;
    case Type_Descriptor::u64: os << "i64"; break;
    case Type_Descriptor::f32: os << "f32"; break;
    case Type_Descriptor::f64: os << "f64"; break;
    default:
        throw std::invalid_argument(
            fmt::format("invalid type descriptor: {}", static_cast<uint8_t>(descriptor)));
    }
    return os;
}

CLI::Option* add_type_descriptor(CLI::App& app,
                                 std::string name,
                                 Type_Descriptor& variable,
                                 std::string description = "",
                                 bool defaulted = false)
{
    CLI::callback_t fun = [&variable](CLI::results_t res) {
        if (res[0] == "i8") {
            variable = Type_Descriptor::i8;
            return true;
        } else if (res[0] == "i16") {
            variable = Type_Descriptor::i16;
            return true;
        } else if (res[0] == "i32") {
            variable = Type_Descriptor::i32;
            return true;
        } else if (res[0] == "i64") {
            variable = Type_Descriptor::i64;
            return true;
        } else if (res[0] == "u8") {
            variable = Type_Descriptor::u8;
            return true;
        } else if (res[0] == "u16") {
            variable = Type_Descriptor::u16;
            return true;
        } else if (res[0] == "u32") {
            variable = Type_Descriptor::u32;
            return true;
        } else if (res[0] == "u64") {
            variable = Type_Descriptor::u64;
            return true;
        } else if (res[0] == "f32") {
            variable = Type_Descriptor::f32;
            return true;
        } else if (res[0] == "f64") {
            variable = Type_Descriptor::f64;
            return true;
        }
        return false;
    };

    CLI::Option *opt = app.add_option(name, fun, description, defaulted);
    opt->type_name("Type_Descriptor")->type_size(1);
    if (defaulted) {
        std::stringstream out;
        out << variable;
        opt->default_str(out.str());
    }
    return opt;
}

class Printable_Vector {
public:
    template<class V>
    explicit Printable_Vector(Vector_View<V> vector)
        : self_(std::make_shared<Model<V>>(vector))
    {}

    static Printable_Vector
    from_type(Type_Descriptor descriptor, gsl::span<std::byte const> memview)
    {
        switch (descriptor) {
        case Type_Descriptor::i8: return Printable_Vector(Vector_View<int8_t>(memview));
        case Type_Descriptor::i16: return Printable_Vector(Vector_View<int16_t>(memview));
        case Type_Descriptor::i32: return Printable_Vector(Vector_View<int32_t>(memview));
        case Type_Descriptor::i64: return Printable_Vector(Vector_View<int64_t>(memview));
        case Type_Descriptor::u8: return Printable_Vector(Vector_View<uint8_t>(memview));
        case Type_Descriptor::u16: return Printable_Vector(Vector_View<uint16_t>(memview));
        case Type_Descriptor::u32: return Printable_Vector(Vector_View<uint32_t>(memview));
        case Type_Descriptor::u64: return Printable_Vector(Vector_View<uint64_t>(memview));
        case Type_Descriptor::f32: return Printable_Vector(Vector_View<float>(memview));
        case Type_Descriptor::f64: return Printable_Vector(Vector_View<double>(memview));
        default:
            throw std::invalid_argument(
                fmt::format("invalid type descriptor: {}", static_cast<uint8_t>(descriptor)));
        };
    }

    std::ostream& print(std::ostream& os) const { return self_->print(os); }
    std::ostream& lookup(std::ptrdiff_t pos, std::ostream& os) const
    {
        return self_->lookup(pos, os);
    }

private:
    struct Printable
    {
        Printable() = default;
        Printable(Printable const&) = default;
        Printable(Printable&&) noexcept = default;
        Printable& operator=(Printable const&) = default;
        Printable& operator=(Printable&&) noexcept = default;
        virtual ~Printable() = default;
        virtual std::ostream& print(std::ostream&) const = 0;
        virtual std::ostream& lookup(std::ptrdiff_t, std::ostream&) const = 0;
    };

    template<class V>
    class Model : public Printable {
    public:
        explicit Model(Vector_View<V> vector) : vector_(std::move(vector)) {}

        std::ostream& print(std::ostream& os) const override
        {
            for (auto const& v : vector_) {
                os << v << '\n';
            }
            return os;
        }

        std::ostream& lookup(std::ptrdiff_t index, std::ostream& os) const override
        {
            os << vector_.at(index) << '\n';
            return os;
        }
    private:
        Vector_View<V> vector_;
    };

private:
    std::shared_ptr<Printable> self_;
};

int main(int argc, char** argv)
{
    std::string file, index;
    Type_Descriptor type;

    CLI::App app{"Operations related to compact tables."};
    app.add_option("vector", file, "Vector file", false)->required()->check(CLI::ExistingFile);
    CLI::Option* type_opt = add_type_descriptor(app, "-t,--type", type, "Element type", false);
    app.require_subcommand(1);
    CLI::App* lookup = app.add_subcommand("lookup", "Print value at a given position");
    lookup->add_option("index", index, "Vector index")->required();
    CLI::App* print = app.add_subcommand("print", "Print all values in a vector");
    CLI11_PARSE(app, argc, argv);

    boost::iostreams::mapped_file_source mapped{file};
    irk::mapped_file_memory_source<std::byte> source{mapped};
    auto view = gsl::make_span<std::byte const>(source.data(), source.size());

    if (not type_opt) {
        type = Type_Descriptor{std::to_integer<uint8_t>(view[0])};
    }

    auto vector = Printable_Vector::from_type(type, view);
    if (*print) {
        vector.print(std::cout);
    } else if (*lookup) {
        vector.lookup(std::stoll(index), std::cout);
    }
}
