#include "search_server.h"
#include "document.h"
#include "string_processing.h"

using namespace std;

    void SearchServer::AddDocument(int document_id, string_view document, DocumentStatus status, const vector<int>& ratings){
        if (documents_.count(document_id) > 0 || !IsValidWord(document) || document_id < 0){
            throw invalid_argument("Invalid symbols, word with minus-symbols only or invalid document id!");
        }

        auto [id_, data_] = documents_.emplace(document_id, DocumentData{
            string(document), ComputeAverageRating(ratings), status
        });

        doc_ids_vector_.push_back(document_id);
        const auto words = SplitIntoWordsNoStop(id_-> second.data_string_);
        const double inv_word_count = 1.0 / words.size();
        for (auto word : words){
            word_frequencies_[document_id][word] += inv_word_count;
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }

    }

    vector<Document> SearchServer::FindTopDocuments(string_view raw_query, DocumentStatus status) const {
        return FindTopDocuments(execution::seq, raw_query, status);
    }

    vector<Document> SearchServer::FindTopDocuments(const execution::sequenced_policy&, string_view raw_query, DocumentStatus status) const {
        return FindTopDocuments(execution::seq, raw_query,
                [status](int document_id, DocumentStatus document_status, int rating){
                    return document_status == status;
                });
    }

    vector<Document> SearchServer::FindTopDocuments(const execution::parallel_policy&, string_view raw_query, DocumentStatus status) const {
        return FindTopDocuments(execution::par, raw_query,
                [status](int document_id, DocumentStatus document_status, int rating){
                    return document_status == status;
                });
    }

    vector<Document> SearchServer::FindTopDocuments(string_view raw_query) const {
        return FindTopDocuments(execution::seq, raw_query, DocumentStatus::ACTUAL);
    }

    vector<Document> SearchServer::FindTopDocuments(const execution::sequenced_policy&, string_view raw_query) const {
        return FindTopDocuments(execution::seq, raw_query, DocumentStatus::ACTUAL);
    }

    vector<Document> SearchServer::FindTopDocuments(const execution::parallel_policy&, string_view raw_query) const {
        return FindTopDocuments(execution::par, raw_query, DocumentStatus::ACTUAL);
    }

    int SearchServer::GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(string_view raw_query, int document_id) const {
        return MatchDocument(execution::seq, raw_query, document_id);
    }

    tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const execution::sequenced_policy&, string_view raw_query, int document_id) const {
        if (documents_.count(document_id) <= 0 || document_id < 0) {
            throw std::invalid_argument("The document ID does not exist"s);
        }
        vector<string_view> matched_words;
        const Query query = ParseQuery(raw_query);
        for (string_view word : query.minus_words){
            if (word_to_document_freqs_.count(word) == 0){
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)){
                return { {}, documents_.at(document_id).status };
                break;
            }
        }

        for (string_view word : query.plus_words){
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        return { matched_words, documents_.at(document_id).status };
    }

    tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const execution::parallel_policy&, string_view raw_query, int document_id) const {

        if (documents_.count(document_id) <= 0 || document_id < 0) {
            throw std::invalid_argument("The document ID does not exist"s);
        }

        const Query& query = ParseQuery(raw_query);

        const auto& lambda = [this, document_id](string_view word) {
            const auto temp = word_to_document_freqs_.find(word);
            return temp != word_to_document_freqs_.end() && temp->second.count(document_id);
        };

        if (any_of(execution::par, query.minus_words.begin(), query.minus_words.end(), lambda)) {
            return { {}, documents_.at(document_id).status };
        }

        vector<string_view> matched_words(query.plus_words.size());

        auto end = copy_if(execution::par, query.plus_words.begin(), query.plus_words.end(), matched_words.begin(), lambda);
        sort(matched_words.begin(), end);
        end = unique(execution::par, matched_words.begin(), end);
        matched_words.erase(end, matched_words.end());

        return { matched_words, documents_.at(document_id).status };
    }

    vector<int>::iterator SearchServer::begin(){
        return doc_ids_vector_.begin();
    }

    vector<int>::iterator SearchServer::end(){
        return doc_ids_vector_.end();
    }

    bool SearchServer::IsValidWord(string_view word){
        return none_of(word.begin(), word.end(), [](char c) {
                return c >= '\0' && c < ' ';
            });
    }

    bool SearchServer::IsStopWord(string_view word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string_view> SearchServer::SplitIntoWordsNoStop(string_view text) const {
        vector<string_view> words;
        for (string_view& word : SplitIntoWords(text)){
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

    SearchServer::QueryWord SearchServer::ParseQueryWord(string_view& text) const {
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

    SearchServer::Query SearchServer::ParseQuery(string_view& text) const {
        Query query;
        for (string_view& word : SplitIntoWords(text)){
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop){
                if (query_word.is_minus){
                    query.minus_words.push_back(query_word.data);
                }
                else {
                    query.plus_words.push_back(query_word.data);
                }
            }
        }
        sort(execution::par, query.minus_words.begin(), query.minus_words.end());
        sort(execution::par, query.plus_words.begin(), query.plus_words.end());
        query.minus_words.erase(unique(query.minus_words.begin(), query.minus_words.end()),query.minus_words.end());
        query.plus_words.erase(unique(query.plus_words.begin(), query.plus_words.end()), query.plus_words.end());
        return query;
    }

    double SearchServer::ComputeWordInverseDocumentFreq(string_view& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    const map<string_view, double>& SearchServer::GetWordFrequencies(int document_id) const{
        if (!documents_.count(document_id)) {
            static map<string_view, double> empty;
            return empty;
        }
        return word_frequencies_.at(document_id);
    }

    void SearchServer::RemoveDocument(int document_id) {
        RemoveDocument(execution::seq, document_id);
    }

    void SearchServer::RemoveDocument(const execution::sequenced_policy&, int document_id) {
        auto temp = find(doc_ids_vector_.begin(), doc_ids_vector_.end(), document_id);

        if (temp == doc_ids_vector_.end()) {
            return;
        }
        else {
            doc_ids_vector_.erase(temp);
        }

        documents_.erase(document_id);
        for_each(word_to_document_freqs_.begin(), word_to_document_freqs_.end(),
                  [&](auto& temp) {
                    temp.second.erase(document_id);
                  });
    }

    void SearchServer::RemoveDocument(const execution::parallel_policy&, int document_id) {
        auto temp = find(execution::par, doc_ids_vector_.begin(), doc_ids_vector_.end(), document_id);

        if (temp == doc_ids_vector_.end()) {
            return;
        }
        else {
            doc_ids_vector_.erase(temp);
        }

        documents_.erase(document_id);
        for_each(execution::par, word_to_document_freqs_.begin(), word_to_document_freqs_.end(),
                  [&](auto& temp) {
                    temp.second.erase(document_id);
                  });
    }
