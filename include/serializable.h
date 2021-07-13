// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#ifndef TURTLECOIN_SERIALIZABLE_H
#define TURTLECOIN_SERIALIZABLE_H

#include <crypto_types.h>
#include <errors.h>

namespace BaseTypes
{
    struct IHashable
    {
        [[nodiscard]] virtual crypto_hash_t hash() const = 0;
    };

    struct IStorable : ISerializable, IHashable
    {
        [[nodiscard]] virtual uint64_t type() const = 0;
    };

    struct ICheckable
    {
        [[nodiscard]] virtual Error check_construction() const = 0;
    };

    struct ITransaction : virtual IStorable, virtual ICheckable
    {
    };
} // namespace BaseTypes

#endif
