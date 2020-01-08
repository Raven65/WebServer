#pragma once

#include <mysql/mysql.h>
#include <string>

class SQLConnection
{
public:
    SQLConnection();
    ~SQLConnection();

    bool initConnection(const char* ip, const char* user, const char* password, const char* db, int port);
    bool sqlQuery(const std::string& sql);
    bool sqlInsert(const std::string& sql);
    MYSQL_ROW fetchRow() { return mysql_fetch_row(res_); }
    void destroyConnection();

private:
    MYSQL mysql_;
    MYSQL_RES* res_;
    MYSQL_ROW row_;

    bool closed_;
};

