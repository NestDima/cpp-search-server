#include "string_processing.h"

using namespace std;

vector<string_view> SplitIntoWords(string_view text){
    vector<string_view> words;
    string_view word;
	string_view temp;

    uint64_t start = text.find_first_not_of(" ");
    const uint64_t end = text.npos;

    while (start != end) {
        uint64_t delimiter = text.find(' ', start);
        if(delimiter == end){
        	temp = text.substr(start);
        }
        else{
        	temp = text.substr(start, delimiter - start);
        }
        words.push_back(temp);
        start = text.find_first_not_of(" ", delimiter);
    }
    return words;
}
