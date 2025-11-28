#pragma once

#include <iterator>
#include <ostream>

namespace hasten::codegen
{

template <class DelimT, class charT = char, class traits = std::char_traits<charT>>
class ostream_joiner
{
public:
    typedef charT char_type;
    typedef traits traits_type;
    typedef std::basic_ostream<charT, traits> ostream_type;
    typedef std::output_iterator_tag iterator_category;
    typedef void value_type;
    typedef void difference_type;
    typedef void pointer;
    typedef void reference;

    ostream_joiner(ostream_type& s, const DelimT& delimiter)
        : out_stream(&s)
        , delim(delimiter)
        , first_element(true)
    {
    }

    ostream_joiner(ostream_type& s, DelimT&& delimiter)
        : out_stream(&s)
        , delim(std::move(delimiter))
        , first_element(true)
    {
    }

    template <typename T>
    ostream_joiner& operator=(const T& value)
    {
        if (!first_element) {
            *out_stream << delim;
        }
        *out_stream << value;
        first_element = false;
        return *this;
    }

    ostream_joiner& operator*() noexcept
    {
        return *this;
    }

    ostream_joiner& operator++() noexcept
    {
        return *this;
    }

    ostream_joiner& operator++(int) noexcept
    {
        return *this;
    }

private:
    ostream_type* out_stream;
    DelimT delim;
    bool first_element;
};

template <class charT, class traits, class DelimT>
ostream_joiner<std::decay_t<DelimT>, charT, traits> make_ostream_joiner(std::basic_ostream<charT, traits>& os,
                                                                        DelimT&& delimiter)
{
    return ostream_joiner<std::decay_t<DelimT>, charT, traits>(os, std::forward<DelimT>(delimiter));
}

}  // namespace hasten::codegen
