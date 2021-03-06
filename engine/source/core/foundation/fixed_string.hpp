#pragma once

#include "definitions_core.hpp"
#include "foundation/string.hpp"
#include "foundation/details/string_entry_pool.hpp"

namespace Engine
{
    class CORE_API FixedString
    {
    public:
        FixedString() = default;

        FixedString(const char_t* str);

        FixedString(const char* str);

        FixedString(const FixedString& other);

        FixedString& operator= (const FixedString& other);

        bool operator== (const FixedString& other) const;

        bool operator!= (const FixedString& other) const;

        String ToString() const;

        FixedEntryId GetEntryId() const {return EntryId;}

        uint32 GetNumber() const;

    private:
        void MakeFixedString(FixedStringView view);

        void MakeFixedString(StringView view);

    private:
        FixedEntryId EntryId{ 0 };
        uint32 Number{ SUFFIX_NUMBER_NONE };
    };

    template <>
    inline uint32 GetHashCode(const FixedString& name)
    {
        return (uint32)(name.GetEntryId()) + ((uint32)(name.GetEntryId() >> 32) * 23) + name.GetNumber();
    }
}
