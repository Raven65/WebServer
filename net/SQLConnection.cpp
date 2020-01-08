#include "SQLConnection.h"
#include <assert.h>

SQLConnection::SQLConnection() : closed_(false) {
    if (mysql_library_init(0, NULL, NULL) != 0 || 
        mysql_init(&mysql_) == NULL ||
        mysql_options(&mysql_, MYSQL_SET_CHARSET_NAME, "utf8mb4") != 0) {
        throw "mysql library init wrong";
    }
}
SQLConnection::~SQLConnection() {
    if (!closed_) {
        destroyConnection();
    }
}

bool SQLConnection::initConnection(const char* ip, const char* user, const char* password, const char* db, int port) {
    if (mysql_real_connect(&mysql_, ip, user, password, db, port, NULL, 0) == NULL) {
        return false;
    }
    return true;
}

bool SQLConnection::sqlQuery(const std::string& sql) {
    if (mysql_query(&mysql_, sql.c_str())) {
        return false;
    } 
    res_ = mysql_store_result(&mysql_);
    return true;
}

void SQLConnection::destroyConnection() {
    mysql_close(&mysql_);
    mysql_server_end();
    closed_ = true;
}
