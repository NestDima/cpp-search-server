# cpp-search-server
Финальный проект: поисковый сервер

Компилляция на g++: 

g++-9 -c document.cpp main.cpp read_input_functions.cpp request_queue.cpp search_server.cpp string_processing.cpp remove_duplicates.cpp process_queries.cpp -std=c++1z -ltbb -lpthread

g++-9 -o prog document.o main.o read_input_functions.o request_queue.o search_server.o string_processing.o remove_duplicates.o process_queries.o -ltbb -lpthread
