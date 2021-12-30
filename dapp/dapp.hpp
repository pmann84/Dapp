#ifndef DAPP_HPP__
#define DAPP_HPP__

#include <iostream>
#include <map>
#include <string>
#include <sstream>
#include <vector>

#include <sqlite3.h>
#include <variant>
#include <any>

namespace db
{
    enum class status_code
    {
        success,
        access_failed,
        error
    };

    class db_row_value
    {
    public:
        db_row_value() = default;

        db_row_value(const std::string& value) : m_row_value(value)
        {
        }

        template<typename T>
        T as()
        {
            return std::any_cast<T>(m_row_value);
        }

        template<>
        uint32_t as<uint32_t>()
        {
            return std::stoul(std::any_cast<std::string>(m_row_value));
        }

        template<>
        uint64_t as<uint64_t>()
        {
            return std::stoull(std::any_cast<std::string>(m_row_value));
        }

    private:
        std::any m_row_value;
    };

    class db_row
    {
    public:
        db_row& operator<<(std::pair<std::string, std::string> kvp)
        {
            m_row_data[kvp.first] = db_row_value(kvp.second);
            return *this;
        }

        db_row_value operator[](const std::string& column_name) const
        {
            // TODO: What do we do if the column doesn't exist in the row data?
            return m_row_data.at(column_name);
        }

    private:
        std::map<std::string, db_row_value> m_row_data;
    };

    class db_data
    {
    public:
        db_data() = default;

        db_data& operator<<(db_row row)
        {
            m_results.push_back(row);
            return *this;
        }

        std::vector<db_row>::iterator begin()
        {
            return m_results.begin();
        }

        std::vector<db_row>::iterator end()
        {
            return m_results.end();
        }

        std::vector<db_row>::const_iterator begin() const
        {
            return m_results.begin();
        }

        std::vector<db_row>::const_iterator end() const
        {
            return m_results.end();
        }

    private:
        std::vector<db_row> m_results;

    };

    struct db_result
    {
        status_code status;
        std::string message;
        std::unique_ptr<db_data> results;
        uint32_t rows_affected = 0;
        uint32_t last_inserted_id = 0;
    };

    class transaction;

    class iconnection
    {
    public:
        virtual db_result execute(const std::string& statement) = 0;
        //virtual void prepare(const std::string& key, const std::string& statement) = 0;
        virtual std::unique_ptr<db::transaction> transaction() = 0;
    };

    // RAII that calls commit on destruction
    class transaction
    {
    public:
        transaction(iconnection& connection) : m_connection(connection)
        {
            auto result = m_connection.execute("BEGIN TRANSACTION;");
            if (result.status != status_code::success)
            {

            }
        }

        ~transaction()
        {
            commit();
        }

        db_result execute(const std::string& statement)
        {
            if (m_transaction_complete)
            {
                return { status_code::error, "Transaction has already been completed or rolled back due to an error. Please start a new one." };
            }

            try
            {
                auto result = m_connection.execute(statement);
                if (result.status != status_code::success)
                {
                    rollback();
                }
                return result;
            }
            catch (...)
            {
                rollback();
                return { status_code::error, "Something went wrong executing statement in transaction!" };
            }
        }

        void commit()
        {
            if (!m_transaction_complete)
            {
                m_connection.execute("COMMIT TRANSACTION;");
                m_transaction_complete = true;
            }
        }

    private:
        void rollback()
        {
            m_connection.execute("ROLLBACK TRANSACTION;");
            m_transaction_complete = true;
        }

    private:
        iconnection& m_connection;
        bool m_transaction_complete = false;
    };

    // RAII connection class, wraps a single always open
    // connection that is closed when the connection class
    // is destroyed
    class connection : public iconnection
    {
    public:
        explicit connection(const std::string& connection_string)
                : m_connection_string(connection_string)
        {
            m_db = nullptr;

            auto result = open();
            if (result.status != status_code::success)
            {
                throw std::runtime_error(result.message);
            }
        }

        ~connection()
        {
            close();
        }

        db_result execute(const std::string& statement) override
        {
            db_data result_data;

            // Note: This callback is called for every row returned from the executed statement
            auto callback = [](void* data, int argc, char** argv, char** azColName) -> int
            {
                auto* exec_results = static_cast<db_data*>(data);
                auto column_count = argc;

                db_row row;
                for (auto i = 0; i < column_count; ++i)
                {
                    row << std::make_pair(azColName[i], argv[i]);
                }
                *exec_results << row;

                // Debug
//                std::stringstream ss;
//                ss << "Row returned -> ";
//                for (auto i = 0; i < column_count; ++i)
//                {
//                    ss << azColName[i] << " -> " << argv[i] << " ";
//                }
//                std::cout << ss.str() << std::endl;
                return 0;
            };

            int result;
            char* error_message;
            result = sqlite3_exec(m_db, statement.c_str(), callback, &result_data, &error_message);

            auto rows_affected = static_cast<uint32_t>(sqlite3_changes(m_db));
            auto last_inserted_id = static_cast<uint32_t>(sqlite3_last_insert_rowid(m_db));

            switch (result)
            {
                case (SQLITE_OK):
                {
                    std::stringstream ss;
                    ss << "Successfully executed SQL statement [" << statement << "]";
                    return { status_code::success,
                             ss.str(),
                             std::make_unique<db_data>(result_data),
                             rows_affected,
                             last_inserted_id };
                }
                default:
                {
                    std::string error_message_str(error_message);
                    sqlite3_free(error_message);
                    return { status_code::error, error_message_str };
                }
            };
        }

        std::unique_ptr<db::transaction> transaction() override
        {
            auto tr = std::make_unique<db::transaction>(*this);
            return std::move(tr);
        }

    private:
        db_result open()
        {
            const char* error_msg;
            auto result = sqlite3_open_v2(
                    m_connection_string.c_str(),
                    &m_db,
                    SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX,
                    nullptr
            );
            switch (result)
            {
                case SQLITE_OK:
                {
                    std::stringstream ss;
                    ss << "Successfully opened database [" << m_connection_string << "]";
                    return {status_code::success, ss.str()};
                }
                case SQLITE_CANTOPEN:
                {
                    error_msg = sqlite3_errmsg(m_db);
                    return {status_code::access_failed, std::string(error_msg)};
                }
                default:
                {
                    error_msg = sqlite3_errmsg(m_db);
                    return {status_code::error, std::string(error_msg)};
                }
            }
        }

        db_result close()
        {
            const char* error_msg;
            auto result = sqlite3_close(m_db);
            switch (result)
            {
                case SQLITE_OK:
                {
                    std::stringstream ss;
                    ss << "Successfully closed database [" << m_connection_string << "]";
                    return {status_code::success, ss.str()};
                }
                default:
                {
                    error_msg = sqlite3_errmsg(m_db);
                    return {status_code::error, error_msg};
                }
            }
        }

    private:
        sqlite3* m_db;
        std::string m_connection_string;
    };
}

#endif // DAPP_HPP__
