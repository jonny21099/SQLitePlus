//
// Created by Kerry Cao on 2020-09-18.
// Copyright (c) 2020 Kerry Cao (kcyq98@gmail.com)
//

#ifndef SQLITEPLUS_SQLITE3_HPP
#define SQLITEPLUS_SQLITE3_HPP

#include <sqlite3.h>
#include <iostream>
#include <memory>
#include <functional>

#include "SQLITE3_QUERY.hpp"

typedef std::vector<std::string> SQLITE_ROW_VECTOR;

class SQLITE3 {
public:
    /**
     * Constructor
     * @param db_name name of database to open
     */
    explicit SQLITE3(const std::string &db_name = ""){
        this->err_msg = nullptr;
        this->db = nullptr;

        // open database if name is provided
        if (!db_name.empty()) {
            int rc = sqlite3_open(db_name.c_str(), &db);
            if (rc != SQLITE_OK) { // check for error
                error_no = 1; // set error code

                sqlite3_close(db);
                throw std::runtime_error("Unable to open database");
            }
            start_transaction();
        }

        // initialize result vector
        result = std::unique_ptr<std::vector<SQLITE_ROW_VECTOR>>(new std::vector<SQLITE_ROW_VECTOR>);
    }

    /**
     * Non-construction-copyable
     */
    SQLITE3(const SQLITE3 &) = delete;

    /**
     * Non-copyable
     * @return SQLITE3
     */
    SQLITE3 &operator=(const SQLITE3 &) = delete;

    /**
     * Destructor
     */
    ~SQLITE3(){
        if (db) {
            sqlite3_close(db);
        }
        if (err_msg) {
            sqlite3_free(err_msg);
        }
    }

    /**
     * Connect to db named db_name
     * @param db_name name of the database to open
     * @return 0 upon success, 1 upon failure
     */
    int open(std::string &db_name){
        if (db) { // can only bind to 1 database
            error_no = 2;
            return 1;
        } else {
            int rc = sqlite3_open(db_name.c_str(), &db);

            if (rc != SQLITE_OK) { // check for error
                error_no = 1; // set error code

                sqlite3_close(db);
                return 1;
            }

            start_transaction();
            return 0; // all good
        }
    }

    /**
     * Commit all change to database, then start a new transaction
     * @return 0 upon success, 1 upon failure
     */
    int commit(){
        int rc = sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &err_msg);
        if (rc != SQLITE_OK) { // check for error
            error_no = 127;
            return 1;
        }
        start_transaction();
        return 0;
    }

    /**
     * Execute query
     * @param query
     * @return 0 upon success, 1 upon failure
     */
    int execute(SQLITE3_QUERY &query){
        // check if database connection is open
        if (!db) {
            error_no = 4;
            return 1;
        }

        // get query from SQLITE3_QUERY
        std::string prepared_query;
        try {
            prepared_query = query.bind().bound_query;
        } catch (std::out_of_range &e) {
            error_no = 3;
            return 1;
        }

        // clear result vector
        result->clear();

        // run query
        int rc = sqlite3_exec(db, prepared_query.c_str(), &exec_callback, &result, &err_msg);
        if (rc != SQLITE_OK) { // check for error
            error_no = 127;
            return 1;
        }
        return 0;
    }

    /**
     * Execute query
     * @param query
     * @return 0 upon success, 1 upon failure
     */
    int execute(std::string &query) {
        // check if database connection is open
        if (!db) {
            error_no = 4;
            return 1;
        }

        // clear result vector
        result->clear();

        // run query
        int rc = sqlite3_exec(db, query.c_str(), &exec_callback, &result, &err_msg);
        if (rc != SQLITE_OK) { // check for error
            error_no = 127;
            return 1;
        }
        return 0;
    }

    /**
     * Execute query
     * @param query
     * @return 0 upon success, 1 upon failure
     */
    int execute(const char* query) {
        // check if database connection is open
        if (!db) {
            error_no = 4;
            return 1;
        }

        // clear result vector
        result->clear();

        // run query
        int rc = sqlite3_exec(db, query, &exec_callback, &result, &err_msg);
        if (rc != SQLITE_OK) { // check for error
            error_no = 127;
            return 1;
        }
        return 0;
    }

    /**
     * Get the number of rows the
     * @return int
     */
    int get_result_row_count(){
        return result->size();
    }

    /**
     * Return the result of a query
     * @return std::vector<std::string>*
     */
    std::vector<SQLITE_ROW_VECTOR>* get_result(){
        return result.get();
    }

    /**
     * Print out result of statement
     */
    void print_result(){
        for (auto &x : *get_result()){ // print results
            std::cout << "|";
            for (auto &y : x){
                std::cout << y << "|";
            }
            std::cout << "\n";
        }
    }

    /**
     * Get sqlite3 pointer, allowing user to expand the functions of SQLite
     * @return db
     */
    sqlite3* getDB(){
        return db;
    }

    /**
     * Read the class wide error_no and print parsed error to std::cerr
     */
    void perror() const{
        switch (error_no) {
            case 0:
                break;
            case 1:
                std::cerr << "SQLITE DATABASE OPEN FAILURE\n";
                break;
            case 2:
                std::cerr << "SQLITE DATABASE ALREADY OPENED, CREATE NEW OBJECT FOR NEW DATABASE\n";
                break;
            case 3:
                std::cerr << "Query Binding Failed\n";
                break;
            case 4:
                std::cerr << "No database connected\n";
                break;
            case 127:
                std::cerr << err_msg << std::endl;
                break;
        }
    }

private:
    /**
     * Begin a new transaction
     * @return 0 upon success, 1 upon failure
     */
    int start_transaction(){
        int rc = sqlite3_exec(db, "BEGIN;", nullptr, nullptr, &err_msg);
        if (rc != SQLITE_OK) { // check for error
            error_no = 127;
            return 1;
        }
        return 0;
    }

    /**
     * Collect results and put into result vector
     * @param ptr pointer to result vector
     * @param argc number of column
     * @param argv result char**
     * @param col_name column name
     * @return 0
     */
    static int exec_callback(void *ptr, int argc, char *argv[], char *col_name[]){
        auto *result = reinterpret_cast<std::unique_ptr<std::vector<SQLITE_ROW_VECTOR>> *>(ptr);

        // get result
        SQLITE_ROW_VECTOR row;
        for(int i = 0; i < argc; i++) {
            row.push_back(std::string(argv[i] ? argv[i] : "NULL"));
        }

        // push SQLITE_ROW_VECTOR to result vector
        result->get()->push_back(row);

        return 0;
    }

public:
    char error_no; // class wide error code

private:
    // sqlite objects
    sqlite3 *db;
    char *err_msg;
    std::unique_ptr<std::vector<SQLITE_ROW_VECTOR>> result;
};


#endif //SQLITEPLUS_SQLITE3_HPP
