#include <execution>
#include <algorithm>

#include "process_queries.h"

using namespace std;

vector<vector<Document>> ProcessQueries(
        const SearchServer& search_server,
        const vector<string>& queries){

    vector<vector<Document>> temp(queries.size());
    transform(execution::par, queries.begin(), queries.end(), temp.begin(),
                   [&search_server](const string& word){
                        return move(search_server.FindTopDocuments(word));
                    });
    return temp;
}

vector<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const vector<string>& queries){

    return transform_reduce(execution::par, queries.begin(), queries.end(), vector<Document>{},
                                [](vector<Document> lhs, vector<Document> const& rhs){
                                    lhs.insert(lhs.end(), rhs.begin(), rhs.end());
                                    return lhs;
                                },
                                [&search_server](const string& query){
                                    return search_server.FindTopDocuments(query);
                                });
}
