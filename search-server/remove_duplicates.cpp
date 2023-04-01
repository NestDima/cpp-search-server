#include "remove_duplicates.h"

using namespace std;

void RemoveDuplicates(SearchServer& search_server) {
    set<int> duplicates;
    map<set<string, less<>>, int> words_doc;

    for (const int document_id : search_server) {
        set<string, less<>> document_words;
        const map<string_view, double> word_frequencies = search_server.GetWordFrequencies(document_id);
        for (auto [word, frequencies] : word_frequencies) {
            (void) frequencies;
            document_words.insert(word);
        }

        if (words_doc.count(document_words)) {
            int prev_id = words_doc[document_words];
            if (prev_id > document_id) {
                words_doc[document_words] = document_id;
                duplicates.insert(prev_id);
            }
            else {
                duplicates.insert(document_id);
            }
        }
        else {
            words_doc[document_words] = document_id;
        }
    }
    for (int document_id : duplicates) {
        cout << "Found duplicate document id " << document_id << endl;
        search_server.RemoveDocument(document_id);
    }
}
