//
// Copyright (C) 2011-2013 Denis Chapligin
// Copyright (C) 2004-2006 Maciej Sobczak, Stephen Hutton, David Courtney
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#define SOCI_DB2_SOURCE
#include "soci-db2.h"
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <sstream>

using namespace soci;
using namespace soci::details;

void db2_standard_use_type_backend::prepare_for_bind(
    void *&data, SQLLEN &size, SQLSMALLINT &sqlType, SQLSMALLINT &cType)
{
    switch (type)
    {
    // simple cases
    case x_short:
        sqlType = SQL_SMALLINT;
        cType = SQL_C_SSHORT;
        size = sizeof(short);
        break;
    case x_integer:
        sqlType = SQL_INTEGER;
        cType = SQL_C_SLONG;
        size = sizeof(int);
        break;
    case x_long_long:
        sqlType = SQL_BIGINT;
        cType = SQL_C_SBIGINT;
        size = sizeof(long long);
        break;
    case x_unsigned_long_long:
        sqlType = SQL_BIGINT;
        cType = SQL_C_UBIGINT;
        size = sizeof(unsigned long long);
        break;
    case x_double:
        sqlType = SQL_DOUBLE;
        cType = SQL_C_DOUBLE;
        size = sizeof(double);
        break;

    // cases that require adjustments and buffer management
    case x_char:
        sqlType = SQL_CHAR;
        cType = SQL_C_CHAR;
        size = sizeof(char) + 1;
        buf = new char[size];
        data = buf;
        indptr = SQL_NTS;
        break;
    case x_stdstring:
    {
        // TODO: No textual value is assigned here!

        std::string* s = static_cast<std::string*>(data);
        sqlType = SQL_LONGVARCHAR;
        cType = SQL_C_CHAR;
        size = s->size() + 1;
        buf = new char[size];
        data = buf;
        indptr = SQL_NTS;
    }
    break;
    case x_stdtm:
        sqlType = SQL_TIMESTAMP;
        cType = SQL_C_TIMESTAMP;
        buf = new char[sizeof(TIMESTAMP_STRUCT)];
        data = buf;
        size = 19; // This number is not the size in bytes, but the number
                   // of characters in the date if it was written out
                   // yyyy-mm-dd hh:mm:ss
        break;

    case x_blob:
        break;
    case x_statement:
    case x_rowid:
        break;
    }
}

void db2_standard_use_type_backend::bind_helper(int &position, void *data, details::exchange_type type)
{
    this->data = data; // for future reference
    this->type = type; // for future reference

    SQLSMALLINT sqlType;
    SQLSMALLINT cType;
    SQLLEN size;

    prepare_for_bind(data, size, sqlType, cType);

    SQLRETURN cliRC = SQLBindParameter(statement_.hStmt,
                                    static_cast<SQLUSMALLINT>(position++),
                                    SQL_PARAM_INPUT,
                                    cType, sqlType, size, 0, data, size, &indptr);

    if (cliRC != SQL_SUCCESS)
    {
        throw db2_soci_error("Error while binding value",cliRC);
    }
}

void db2_standard_use_type_backend::bind_by_pos(
    int &position, void *data, exchange_type type, bool /* readOnly */)
{
    bind_helper(position, data, type);

}

void db2_standard_use_type_backend::bind_by_name(
    std::string const &name, void *data, exchange_type type, bool /* readOnly */)
{
    int position = -1;
    int count = 1;

    for (std::vector<std::string>::iterator it = statement_.names.begin();
         it != statement_.names.end(); ++it)
    {
        if (*it == name)
        {
            position = count;
            break;
        }
        count++;
    }

    if (position != -1)
    {
        bind_helper(position, data, type);
    }
    else
    {
        std::ostringstream ss;
        ss << "Unable to find name '" << name << "' to bind to";
        throw soci_error(ss.str().c_str());
    }
}

void db2_standard_use_type_backend::pre_use(indicator const *ind)
{
    // first deal with data
    if (type == x_char)
    {
        char *c = static_cast<char*>(data);
        buf[0] = *c;
        buf[1] = '\0';
    }
    else if (type == x_stdstring)
    {
        std::string *s = static_cast<std::string *>(data);
        std::size_t const bufSize = s->size() + 1;

        std::size_t const sSize = s->size();
        std::size_t const toCopy = sSize < bufSize -1 ? sSize + 1 : bufSize - 1;
        strncpy(buf, s->c_str(), toCopy);
        buf[toCopy] = '\0';
    }
    else if (type == x_stdtm)
    {
        std::tm *t = static_cast<std::tm *>(data);
        TIMESTAMP_STRUCT * ts = reinterpret_cast<TIMESTAMP_STRUCT*>(buf);

        ts->year = static_cast<SQLSMALLINT>(t->tm_year + 1900);
        ts->month = static_cast<SQLUSMALLINT>(t->tm_mon + 1);
        ts->day = static_cast<SQLUSMALLINT>(t->tm_mday);
        ts->hour = static_cast<SQLUSMALLINT>(t->tm_hour);
        ts->minute = static_cast<SQLUSMALLINT>(t->tm_min);
        ts->second = static_cast<SQLUSMALLINT>(t->tm_sec);
        ts->fraction = 0;
    }

    // then handle indicators
    if (ind != NULL && *ind == i_null)
    {
        indptr = SQL_NULL_DATA; // null
    }
}

void db2_standard_use_type_backend::post_use(bool /*gotData*/, indicator* /*ind*/)
{

}

void db2_standard_use_type_backend::clean_up()
{
    if (buf != NULL)
    {
        delete [] buf;
        buf = NULL;
    }
}
