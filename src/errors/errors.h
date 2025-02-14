// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#ifndef TURTLECOIN_ERRORS_H
#define TURTLECOIN_ERRORS_H

#include <json_helper.h>
#include <sstream>
#include <string>

#define MAKE_ERROR(code) Error(code, __LINE__, __FILE__)
#define MAKE_ERROR_MSG(code, message) Error(code, message, __LINE__, __FILE__)

enum ErrorCode
{
    SUCCESS = 0,

    // (de)serialization error code(s)
    JSON_PARSE_ERROR,

    // networking error code(s)
    UPNP_FAILURE,
    UPNP_NOT_SUPPORTED,
    ZMQ_CONNECT_ERROR,
    ZMQ_BIND_ERROR,
    ZMQ_GENERIC_ERROR,
    P2P_SEED_CONNECT,
    P2P_DUPE_CONNECT,
    HTTP_BODY_REQUIRED_BUT_NOT_FOUND,

    // peer list error code(s)
    PEERLIST_ADD_FAILURE,

    // address encoding error code(s)
    BASE58_DECODE,
    ADDRESS_PREFIX_MISMATCH,
    ADDRESS_DECODE,

    // database error code(s)
    DB_EMPTY,
    DB_BLOCK_NOT_FOUND,
    DB_TRANSACTION_NOT_FOUND,
    DB_TRANSACTION_OUTPUT_NOT_FOUND,

    // block error code(s)
    BLOCK_TXN_ORDER,
    BLOCK_TXN_MISMATCH,

    // transaction error code(s)
    UNKNOWN_TRANSACTION_TYPE,

    // transaction validation error(s)
    TX_NOT_FOUND,
    TX_INVALID_VERSION,
    TX_KEY_IMAGE_ALREADY_EXISTS,
    TX_DUPLICATE_KEY_IMAGE,
    TX_INVALID_KEY_IMAGE,
    TX_MINIMUM_POW,
    TX_LOW_FEE,
    TX_MISSING_FEE,
    TX_EXTRA_TOO_LARGE,
    TX_KEYPAIR_MISMATCH,
    TX_PUBLIC_KEY,
    TX_SECRET_KEY,
    TX_STAKE_NO_AMOUNT,
    TX_STAKER_ID,
    TX_RECALL_VIEW_SIGNATURE,
    TX_RECALL_SPEND_SIGNATURE,
    TX_RECALL_STAKE_TX_HASH,
    TX_OUTPUT_PUBLIC_EPHEMERAL,
    TX_OUTPUT_AMOUNT,
    TX_OUTPUT_COMMITMENT,
    TX_OUTPUT_LOCKED,
    TX_STAKER_REWARD_AMOUNT,
    TX_STAKER_REWARD_ID,
    TX_INVALID_RANGE_PROOF,
    TX_SIG_SIZE_MISMATCH,
    TX_INVALID_SIGNATURE,
    TX_INVALID_OUTPUT_COUNT,
    TX_INVALID_INPUT_COUNT,
    TX_PUBLIC_VIEW_KEY_NOT_FOUND,
    TX_PUBLIC_SPEND_KEY_NOT_FOUND,
    TX_INVALID_PSEUDO_COMMITMENT_COUNT,
    TX_COMMITMENTS_DO_NOT_BALANCE,
    TX_INVALID_RING_SIGNATURE,
    TX_GENESIS_ALREADY_EXISTS,
    TX_STAKING_PUBLIC_KEYS_REUSE,

    // staking error code(s)
    STAKING_CANDIDATE_ALREADY_EXISTS,
    STAKING_CANDIDATE_NOT_FOUND,
    STAKING_CANDIDATE_AMOUNT_INVALID,
    STAKING_STAKER_NOT_FOUND,
    STAKING_STAKE_AMOUNT,

    // AES error code(s)
    AES_WRONG_PASSWORD,
    AES_DECRYPTION_ERROR,

    /**
     * Do not change LMDB values as they map directly to LMDB return codes
     * See: http://www.lmdb.tech/doc/group__errors.html
     */
    LMDB_ERROR = -40000,
    LMDB_EMPTY = -39999,
    LMDB_KEYEXIST = -30799,
    LMDB_NOTFOUND = -30798,
    LMDB_PAGE_NOTFOUND = -30797,
    LMDB_CORRUPTED = -30796,
    LMDB_PANIC = -30795,
    LMDB_VERSION_MISMATCH = -30794,
    LMDB_INVALID = -30793,
    LMDB_MAP_FULL = -30792,
    LMDB_DBS_FULL = -30791,
    LMDB_READERS_FULL = -30790,
    LMDB_TLS_FULL = -30789,
    LMDB_TXN_FULL = -30788,
    LMDB_CURSOR_FULL = -30787,
    LMDB_PAGE_FULL = -30786,
    LMDB_MAP_RESIZED = -30785,
    LMDB_INCOMPATIBLE = -30784,
    LMDB_BAD_RSLOT = -30783,
    LMDB_BAD_TXN = -30782,
    LMDB_BAD_VALSIZE = -30781,
    LMDB_BAD_DBI = -30780
};

class Error
{
  public:
    Error(): m_error_code(SUCCESS) {}

    /**
     * Creates an error with the specified code
     *
     * @param code
     * @param line_number
     * @param file_name
     */
    Error(const ErrorCode &code, size_t line_number = 0, const std::string file_name = ""):
        m_error_code(code), m_line_number(line_number), m_file_name(file_name)
    {
    }

    /**
     * Creates an error with the specified code and a custom error message
     *
     * @param code
     * @param custom_message
     * @param line_number
     * @param file_name
     */
    Error(
        const ErrorCode &code,
        const std::string &custom_message,
        size_t line_number = 0,
        const std::string file_name = ""):
        m_error_code(code), m_custom_error_message(custom_message), m_line_number(line_number), m_file_name(file_name)
    {
    }

    /**
     * Creates an error with the specified code
     *
     * @param code
     * @param line_number
     * @param file_name
     */
    Error(const int &code, size_t line_number = 0, const std::string file_name = ""):
        m_error_code(static_cast<ErrorCode>(code)), m_line_number(line_number), m_file_name(file_name)
    {
    }

    /**
     * Creates an error with the specified code and a custom error message
     *
     * @param code
     * @param custom_message
     * @param line_number
     * @param file_name
     */
    Error(const int &code, const std::string &custom_message, size_t line_number = 0, const std::string file_name = ""):
        m_error_code(static_cast<ErrorCode>(code)),
        m_custom_error_message(custom_message),
        m_line_number(line_number),
        m_file_name(file_name)
    {
    }

    bool operator==(const ErrorCode &code) const
    {
        return code == m_error_code;
    }

    bool operator==(const Error &error) const
    {
        return error.code() == m_error_code;
    }

    bool operator!=(const ErrorCode &code) const
    {
        return !(code == m_error_code);
    }

    bool operator!=(const Error &error) const
    {
        return !(error.code() == m_error_code);
    }

    explicit operator bool() const
    {
        return m_error_code != SUCCESS;
    }

    /**
     * Returns the error code
     *
     * @return
     */
    [[nodiscard]] ErrorCode code() const;

    /**
     * Return the filename of the file where the error was created
     *
     * @return
     */
    [[nodiscard]] std::string file_name() const;

    /**
     * Return the line number of the file where the error was created
     *
     * @return
     */
    [[nodiscard]] size_t line() const;

    /**
     * Provides the error as a JSON object
     *
     * @param writer
     */
    JSON_TO_FUNC(toJSON);

    /**
     * Returns the error message of the instance
     *
     * @return
     */
    [[nodiscard]] std::string to_string() const;

  private:
    ErrorCode m_error_code;

    size_t m_line_number;

    std::string m_file_name;

    std::string m_custom_error_message;
};

inline std::ostream &operator<<(std::ostream &os, const Error &error)
{
    if (!error.file_name().empty())
    {
        os << error.file_name() << " L#" << error.line() << " ";
    }

    os << "Error #" << std::to_string(error.code()) << ": " << error.to_string();

    return os;
}

#endif // TURTLECOIN_ERRORS_H
