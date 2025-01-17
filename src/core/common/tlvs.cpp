/*
 *  Copyright (c) 2016, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   This file implements common methods for manipulating MLE TLVs.
 */

#include "tlvs.hpp"

#include "common/code_utils.hpp"
#include "common/debug.hpp"
#include "common/message.hpp"

namespace ot {

uint32_t Tlv::GetSize(void) const
{
    return IsExtended() ? sizeof(ExtendedTlv) + As<ExtendedTlv>(this)->GetLength() : sizeof(Tlv) + GetLength();
}

uint8_t *Tlv::GetValue(void)
{
    return reinterpret_cast<uint8_t *>(this) + (IsExtended() ? sizeof(ExtendedTlv) : sizeof(Tlv));
}

const uint8_t *Tlv::GetValue(void) const
{
    return reinterpret_cast<const uint8_t *>(this) + (IsExtended() ? sizeof(ExtendedTlv) : sizeof(Tlv));
}

Error Tlv::AppendTo(Message &aMessage) const
{
    return aMessage.AppendBytes(this, static_cast<uint16_t>(GetSize()));
}

Error Tlv::FindTlv(const Message &aMessage, uint8_t aType, uint16_t aMaxSize, Tlv &aTlv)
{
    Error    error;
    uint16_t offset;
    uint16_t size;

    SuccessOrExit(error = Find(aMessage, aType, &offset, &size, nullptr));

    if (aMaxSize > size)
    {
        aMaxSize = size;
    }

    aMessage.ReadBytes(offset, &aTlv, aMaxSize);

exit:
    return error;
}

Error Tlv::FindTlvOffset(const Message &aMessage, uint8_t aType, uint16_t &aOffset)
{
    return Find(aMessage, aType, &aOffset, nullptr, nullptr);
}

Error Tlv::FindTlvValueOffset(const Message &aMessage, uint8_t aType, uint16_t &aValueOffset, uint16_t &aLength)
{
    Error    error;
    uint16_t offset;
    uint16_t size;
    bool     isExtendedTlv;

    SuccessOrExit(error = Find(aMessage, aType, &offset, &size, &isExtendedTlv));

    if (!isExtendedTlv)
    {
        aValueOffset = offset + sizeof(Tlv);
        aLength      = size - sizeof(Tlv);
    }
    else
    {
        aValueOffset = offset + sizeof(ExtendedTlv);
        aLength      = size - sizeof(ExtendedTlv);
    }

exit:
    return error;
}

Error Tlv::Find(const Message &aMessage, uint8_t aType, uint16_t *aOffset, uint16_t *aSize, bool *aIsExtendedTlv)
{
    // This static method searches within a `aMessage` for a TLV type
    // `aType` and outputs the TLV offset, size, and whether or not it
    // is an Extended TLV.
    //
    // A `nullptr` pointer can be used for output parameters `aOffset`,
    // `aSize`, or `aIsExtendedTlv` if the parameter is not required.
    //
    // Returns `kErrorNone` when found, otherwise `kErrorNotFound`.

    Error    error        = kErrorNotFound;
    uint16_t offset       = aMessage.GetOffset();
    uint16_t remainingLen = aMessage.GetLength();
    Tlv      tlv;
    uint32_t size;

    VerifyOrExit(offset <= remainingLen);
    remainingLen -= offset;

    while (true)
    {
        SuccessOrExit(aMessage.Read(offset, tlv));

        if (tlv.mLength != kExtendedLength)
        {
            size = tlv.GetSize();
        }
        else
        {
            ExtendedTlv extTlv;

            SuccessOrExit(aMessage.Read(offset, extTlv));

            VerifyOrExit(extTlv.GetLength() <= (remainingLen - sizeof(ExtendedTlv)));
            size = extTlv.GetSize();
        }

        VerifyOrExit(size <= remainingLen);

        if (tlv.GetType() == aType)
        {
            if (aOffset != nullptr)
            {
                *aOffset = offset;
            }

            if (aSize != nullptr)
            {
                *aSize = static_cast<uint16_t>(size);
            }

            if (aIsExtendedTlv != nullptr)
            {
                *aIsExtendedTlv = (tlv.mLength == kExtendedLength);
            }

            error = kErrorNone;
            ExitNow();
        }

        offset += size;
        remainingLen -= size;
    }

exit:
    return error;
}

Error Tlv::ReadStringTlv(const Message &aMessage, uint16_t aOffset, uint8_t aMaxStringLength, char *aValue)
{
    Error    error = kErrorNone;
    uint16_t valueOffset;
    uint16_t length;

    SuccessOrExit(error = ReadTlv(aMessage, aOffset, length, valueOffset));

    length = Min(length, static_cast<uint16_t>(aMaxStringLength));

    aMessage.ReadBytes(valueOffset, aValue, length);
    aValue[length + 1] = '\0';

exit:
    return error;
}

template <typename UintType> Error Tlv::ReadUintTlv(const Message &aMessage, uint16_t aOffset, UintType &aValue)
{
    Error error;

    SuccessOrExit(error = ReadTlvValue(aMessage, aOffset, &aValue, sizeof(aValue)));
    aValue = Encoding::BigEndian::HostSwap<UintType>(aValue);

exit:
    return error;
}

// Explicit instantiations of `ReadUintTlv<>()`
template Error Tlv::ReadUintTlv<uint8_t>(const Message &aMessage, uint16_t aOffset, uint8_t &aValue);
template Error Tlv::ReadUintTlv<uint16_t>(const Message &aMessage, uint16_t aOffset, uint16_t &aValue);
template Error Tlv::ReadUintTlv<uint32_t>(const Message &aMessage, uint16_t aOffset, uint32_t &aValue);

Error Tlv::ReadTlv(const Message &aMessage, uint16_t aOffset, uint16_t &aLength, uint16_t &aValueOffset)
{
    Error    error;
    Tlv      tlv;
    uint32_t size;

    SuccessOrExit(error = aMessage.Read(aOffset, tlv));

    if (!tlv.IsExtended())
    {
        aValueOffset = aOffset + sizeof(Tlv);
        aLength      = tlv.GetLength();
        size         = sizeof(Tlv) + aLength;
    }
    else
    {
        ExtendedTlv extTlv;

        SuccessOrExit(error = aMessage.Read(aOffset, extTlv));
        aValueOffset = aOffset + sizeof(ExtendedTlv);
        aLength      = extTlv.GetLength();
        size         = sizeof(ExtendedTlv) + aLength;
    }

    VerifyOrExit(aOffset + size <= aMessage.GetLength(), error = kErrorParse);

exit:
    return error;
}

Error Tlv::ReadTlvValue(const Message &aMessage, uint16_t aOffset, void *aValue, uint8_t aMinLength)
{
    Error    error;
    uint16_t valueOffset;
    uint16_t length;

    SuccessOrExit(error = ReadTlv(aMessage, aOffset, length, valueOffset));

    VerifyOrExit(length >= aMinLength, error = kErrorParse);

    aMessage.ReadBytes(valueOffset, aValue, aMinLength);

exit:
    return error;
}

Error Tlv::FindStringTlv(const Message &aMessage, uint8_t aType, uint8_t aMaxStringLength, char *aValue)
{
    Error    error = kErrorNone;
    uint16_t offset;

    SuccessOrExit(error = FindTlvOffset(aMessage, aType, offset));
    error = ReadStringTlv(aMessage, offset, aMaxStringLength, aValue);

exit:
    return error;
}

template <typename UintType> Error Tlv::FindUintTlv(const Message &aMessage, uint8_t aType, UintType &aValue)
{
    Error    error = kErrorNone;
    uint16_t offset;

    SuccessOrExit(error = FindTlvOffset(aMessage, aType, offset));
    error = ReadUintTlv<UintType>(aMessage, offset, aValue);

exit:
    return error;
}

// Explicit instantiations of `FindUintTlv<>()`
template Error Tlv::FindUintTlv<uint8_t>(const Message &aMessage, uint8_t aType, uint8_t &aValue);
template Error Tlv::FindUintTlv<uint16_t>(const Message &aMessage, uint8_t aType, uint16_t &aValue);
template Error Tlv::FindUintTlv<uint32_t>(const Message &aMessage, uint8_t aType, uint32_t &aValue);

Error Tlv::FindTlv(const Message &aMessage, uint8_t aType, void *aValue, uint8_t aLength)
{
    Error    error;
    uint16_t offset;
    uint16_t length;

    SuccessOrExit(error = FindTlvValueOffset(aMessage, aType, offset, length));
    VerifyOrExit(length >= aLength, error = kErrorParse);
    aMessage.ReadBytes(offset, aValue, aLength);

exit:
    return error;
}

Error Tlv::AppendStringTlv(Message &aMessage, uint8_t aType, uint8_t aMaxStringLength, const char *aValue)
{
    uint16_t length = (aValue == nullptr) ? 0 : StringLength(aValue, aMaxStringLength);

    return AppendTlv(aMessage, aType, aValue, static_cast<uint8_t>(length));
}

template <typename UintType> Error Tlv::AppendUintTlv(Message &aMessage, uint8_t aType, UintType aValue)
{
    UintType value = Encoding::BigEndian::HostSwap<UintType>(aValue);

    return AppendTlv(aMessage, aType, &value, sizeof(UintType));
}

// Explicit instantiations of `AppendUintTlv<>()`
template Error Tlv::AppendUintTlv<uint8_t>(Message &aMessage, uint8_t aType, uint8_t aValue);
template Error Tlv::AppendUintTlv<uint16_t>(Message &aMessage, uint8_t aType, uint16_t aValue);
template Error Tlv::AppendUintTlv<uint32_t>(Message &aMessage, uint8_t aType, uint32_t aValue);

Error Tlv::AppendTlv(Message &aMessage, uint8_t aType, const void *aValue, uint8_t aLength)
{
    Error error = kErrorNone;
    Tlv   tlv;

    OT_ASSERT(aLength <= Tlv::kBaseTlvMaxLength);

    tlv.SetType(aType);
    tlv.SetLength(aLength);
    SuccessOrExit(error = aMessage.Append(tlv));

    VerifyOrExit(aLength > 0);
    error = aMessage.AppendBytes(aValue, aLength);

exit:
    return error;
}

} // namespace ot
