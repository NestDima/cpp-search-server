#include <execution>
#include <algorithm>

#include "process_queries.h"

using namespace std;

vector<vector<Document>> ProcessQueries(
		const SearchServer& search_server,
		const vector<string>& queries){

    vector<vector<Document>> temp(queries.size());
/*  //slow realisation
    for (const std::string& query : queries) {
        temp.push_back(search_server.FindTopDocuments(query));
    }
*/
     transform(execution::par,
                   queries.begin(),
                   queries.end(),
                   temp.begin(),
                   [&search_server](const string& word){
                        return move(search_server.FindTopDocuments(word));
                    });
    return temp;
}
