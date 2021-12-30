# Dapp

## Overview
This is a simple wrapper over the SQLite C interface to make it a bit easier to use within modern C++ applications. This uses a RAII connection, that represents a single always open
connection that is closed when the object is destroyed. Transactionality is also currently supported via an RAII interface.

## Usage
Below are some basic examples of how you can use this in your application
### Basic Query
```c++
db::connection conn("my;connection;string");
auto response = conn.execute("select * from mytable");
```

### Accessing Returned Results
Data is accessed in rows whose columns are then accessed via their name from the resulting response object from the execute command
```c++
// Check call was successful
if (response.status == db::status_code::success)
{   
    // Iterate over all the rows
    for (auto result : *response.results)
    {
        // Obtain column values via name lookup
        auto id = result["id"].as<uint32_t>();
        auto name = result["name"].as<std::string>();
    }
}
```

### Transactions
```c++
db::connection conn(m_connection_string);
{
    // Transaction commits results on destruction
    auto tr = conn.transaction();
    tr->execute("insert into mytable values (1, \"a\")")
    tr->execute("insert into myOtherTable values (2, \"b\")")
}
```