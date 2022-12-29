#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <optional>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double epsilon = 1e-6;

string ReadLine(){
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber(){
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text){
    vector<string> words;
    string word;
    for (const char c : text){
        if (c == ' '){
            if (!word.empty()){
                words.push_back(word);
                word.clear();
            }
        }
        else {
            word += c;
        }
    }
    if (!word.empty()){
        words.push_back(word);
    }

    return words;
}

struct Document {
    Document() = default;

    Document(int id, double relevance, int rating)
        : id(id)
        , relevance(relevance)
        , rating(rating) {
    }

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

template <typename StringContainer>
set<string> MakeUniqueNonEmptyStrings(const StringContainer& strings){
    set<string> non_empty_strings;
    for (const string& str : strings){
        if (!str.empty()){
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words)){
            for (string word : stop_words){
                if (!IsValidWord(word)){
                    stop_words_.clear();
                    throw invalid_argument("Invalid symbols or word with minus-symbols only!");                
                }
            }
        }

    explicit SearchServer(const string& stop_words_text)
        : SearchServer(
            SplitIntoWords(stop_words_text)){
            }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings){
        vector<string> words;
        if ((uniq_ids_store_.count(document_id) !=0 || (!SplitIntoWordsNoStop(document, words))) || (document_id < 0)){
            throw invalid_argument("Invalid symbols, word with minus-symbols only or invalid document id!");
        }
        uniq_ids_store_.insert(document_id);
        count_to_id_.push_back(document_id);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words){
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    }

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query,
        DocumentPredicate document_predicate) const {
        Query query;
        if (ParseQuery(raw_query, query)){
            auto matched_documents = FindAllDocuments(query, document_predicate);
            sort(matched_documents.begin(), matched_documents.end(),
                [](const Document& lhs, const Document& rhs) {
                    if (abs(lhs.relevance - rhs.relevance) < epsilon) {
                        return lhs.rating > rhs.rating;
                    }
                    else {
                        return lhs.relevance > rhs.relevance;
                    }
                });
            if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
                matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
            }
            return matched_documents;
        }
        throw invalid_argument("Invalid symbols or word with minus-symbols only!");
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        return FindTopDocuments(
            raw_query, [status](int document_id, DocumentStatus document_status, int rating){
                return document_status == status;
            });
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        vector<string> matched_words;
        Query query;
        if (ParseQuery(raw_query, query)){
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
        throw invalid_argument("Invalid symbols or word with minus-symbols only!");

    }

    int GetDocumentId(int index){
        if (index >= 0 && index <= GetDocumentCount()){
            return count_to_id_.at(index);
        }
        throw out_of_range("Invalid document id!");
    }


private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;
    set<int> uniq_ids_store_;
    vector<int> count_to_id_;

    static bool IsValidWord(const string& word){
        return none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
            });
    }

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    bool SplitIntoWordsNoStop(const string& text, vector<string>& words) const {
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                if (IsValidWord(word))
                    words.push_back(word);
                else
                    return false;
            }
        }
        return true;
    }

    static int ComputeAverageRating(const vector<int>& ratings){
        if (ratings.empty()){
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings){
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    bool ParseQuery(const string& text, Query& query_words) const {
        vector<string> words;
        if (SplitIntoWordsNoStop(text,words)){
            for (string& word : words) {
                if (word[0] != '-')
                    query_words.plus_words.insert(word);
                else {
                    if (!word.substr(1).empty() && word[1] != '-') {
                        word = word.substr(1);
                        query_words.minus_words.insert(word);
                    }
                    else
                        return false;
                }
            }
            return true;
        }
        else
            return false;
    }

    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query,
        DocumentPredicate document_predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words){
            if (word_to_document_freqs_.count(word) == 0){
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)){
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)){
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words){
            if (word_to_document_freqs_.count(word) == 0){
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)){
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance){
            matched_documents.push_back(
                { document_id, relevance, documents_.at(document_id).rating });
        }
        return matched_documents;
    }

};

int main(){

} 