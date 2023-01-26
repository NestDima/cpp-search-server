#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <deque>

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

ostream& operator << (ostream& out, const Document search){
   return out << "{ document_id = " << search.id << ", relevance = " << search.relevance << ", rating = " << search.rating << " }";
}

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

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query,
        DocumentPredicate document_predicate) const {
        const Query query = ParseQuery(raw_query);
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
    vector<int> count_to_id_;

    static bool IsValidWord(const string& word){
        return none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
            });
    }

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)){
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
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

    QueryWord ParseQueryWord(string text) const {
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

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
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
                (void)_;
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

//небольшой класс, который позволит работать с парами итераторов (начало и конец страницы
template<typename Iterator>
    class IteratorRange {
    public:
        IteratorRange(const Iterator begin, const Iterator end)
        : page_(begin, end)
        {}
        auto begin() const {
            return page_.first;
        }
        auto end() const {
            return page_.second;
        }
        size_t size() const {
            return (distance(begin, end - 1));
        }

    private:
        pair<Iterator, Iterator> page_;
    };

template<typename Iterator>
ostream& operator <<(ostream& os, const IteratorRange<Iterator>& page ) {
    for (auto it = page.begin(); it != page.end(); ++it) {
        os << *it;
    }
    return os;
}

//класс, отвечающий за разделение по страницам
template <typename Iterator>
    class Paginator {
    public:
        Paginator(Iterator begin, Iterator end, int size) : page_size_(size){
            Iterator begining = begin;
            Iterator ending = begin;
            while(distance(ending, end) > size) {
                advance(ending, size);
                pages_.push_back(IteratorRange(begining, ending));
                begining = ending;
            }
            if (begining < end) {
                pages_.push_back(IteratorRange(ending, end));
            }
        }

    auto begin() const {
        return pages_.begin();
    }
    auto end() const {
        return pages_.end();
    }
    int size() const {
        return page_size_;
    }

    private:
        int page_size_;
        vector<IteratorRange<Iterator>> pages_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}


//класс определяющий поступающие запросы в очередь и ведущий статистику по их обработке
class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server)
        : search_server_(search_server)
        , requests_without_resault_(0)
        , current_time_(0){
    }
    template <typename DocumentPredicate>
    vector<Document> AddFindRequest(const string& raw_query, DocumentPredicate document_predicate) {
        ++current_time_;
        const auto result = search_server_.FindTopDocuments(raw_query, document_predicate);
        RemoveOld();
        uint64_t res = result.size();
        requests_.push_back({current_time_, res});
        if (0 == res) {
            ++requests_without_resault_;
        }
        return {};
    }
    vector<Document> AddFindRequest(const string& raw_query, DocumentStatus status) {
        ++current_time_;
        const auto result = search_server_.FindTopDocuments(raw_query, status);
        RemoveOld();
        uint64_t res = result.size();
        requests_.push_back({current_time_, res});
        if (0 == res) {
            ++requests_without_resault_;
        }
        return {};
    }
    vector<Document> AddFindRequest(const string& raw_query) {
        ++current_time_;
        const auto result = search_server_.FindTopDocuments(raw_query);
        RemoveOld();
        uint64_t res = result.size();
        requests_.push_back({current_time_, res});
        if (0 == res) {
            ++requests_without_resault_;
        }
        return {};
    }
    int GetNoResultRequests() const {
        return requests_without_resault_;
    }

    void RemoveOld(){
        while (!requests_.empty() && min_in_day_ <= current_time_ - requests_.front().time) {
            if (0 == requests_.front().results) {
                --requests_without_resault_;
            }
            requests_.pop_front();
        }
    }
private:
    struct QueryResult {
        uint64_t time;
        uint64_t results;
    };
    deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    const SearchServer& search_server_;
    int requests_without_resault_;
    uint64_t current_time_;
};

int main() {
    SearchServer search_server("and in at"s);
    RequestQueue request_queue(search_server);
    search_server.AddDocument(1, "curly cat curly tail"s, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "curly dog and fancy collar"s, DocumentStatus::ACTUAL, {1, 2, 3});
    search_server.AddDocument(3, "big cat fancy collar "s, DocumentStatus::ACTUAL, {1, 2, 8});
    search_server.AddDocument(4, "big dog sparrow Eugene"s, DocumentStatus::ACTUAL, {1, 3, 2});
    search_server.AddDocument(5, "big dog sparrow Vasiliy"s, DocumentStatus::ACTUAL, {1, 1, 1});
    // 1439 запросов с нулевым результатом
    for (int i = 0; i < 1439; ++i) {
        request_queue.AddFindRequest("empty request"s);
    }
    // все еще 1439 запросов с нулевым результатом
    request_queue.AddFindRequest("curly dog"s);
    // новые сутки, первый запрос удален, 1438 запросов с нулевым результатом
    request_queue.AddFindRequest("big collar"s);
    // первый запрос удален, 1437 запросов с нулевым результатом
    request_queue.AddFindRequest("sparrow"s);
    cout << "Total empty requests: "s << request_queue.GetNoResultRequests() << endl;
    return 0;
}
