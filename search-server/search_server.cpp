#include "search_server.h"
#include "document.h"
#include "string_processing.h"

using namespace std;

SearchServer::SearchServer(const string& stop_words_text)
        : SearchServer(
            SplitIntoWords(stop_words_text)){
                }

    void SearchServer::AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings){
        if (find(count_to_id_.begin(), count_to_id_.end(), document_id) != count_to_id_.end() || !IsValidWord(document) || document_id < 0){
            throw invalid_argument("Invalid symbols, word with minus-symbols only or invalid document id!");
        }
        count_to_id_.insert(count_to_id_.end(), document_id);
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words){
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    }

    vector<Document> SearchServer::FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        return FindTopDocuments(
            raw_query, [status](int document_id, DocumentStatus document_status, int rating){
                return document_status == status;
            });
    }

    vector<Document> SearchServer::FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    int SearchServer::GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> SearchServer::MatchDocument(const string& raw_query, int document_id) const {
        vector<string> matched_words;
        const Query query = ParseQuery(raw_query);
        for (const string& word : query.plus_words){
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words){
            if (word_to_document_freqs_.count(word) == 0){
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)){
                matched_words.clear();
                break;
            }
        }
        return { matched_words, documents_.at(document_id).status };
    }

    int SearchServer::GetDocumentId(int index){
        if (index >= 0 && index <= GetDocumentCount()){
            return count_to_id_.at(index);
        }
        throw out_of_range("Invalid document id!");
    }

    bool SearchServer::IsValidWord(const string& word){
        return none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
            });
    }

    bool SearchServer::IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SearchServer::SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)){
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    int SearchServer::ComputeAverageRating(const vector<int>& ratings){
        if (ratings.empty()){
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings){
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    SearchServer::QueryWord SearchServer::ParseQueryWord(string text) const {
        bool is_minus = false;
        if(!IsValidWord(text)){
            throw invalid_argument("Invalid symbols or word with minus-symbols only!");
        }
        if (text[0] == '-') {
            if(!text.substr(1).empty() && text[1] != '-'){
                is_minus = true;
                 text = text.substr(1);
            }
            else{
                throw invalid_argument("Invalid symbols or word with minus-symbols only!");
            }
        }
        return { text, is_minus, IsStopWord(text) };
    }

    SearchServer::Query SearchServer::ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)){
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop){
                if (query_word.is_minus){
                    query.minus_words.insert(query_word.data);
                }
                else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    double SearchServer::ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }