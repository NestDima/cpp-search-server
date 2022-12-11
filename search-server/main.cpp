#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <numeric>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

const double epsilon = 1e-6;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status,
                     const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    }

    vector<Document> FindTopDocuments(const string &raw_query, DocumentStatus status) const {
            return FindTopDocuments(raw_query,
                [status](int id, const DocumentStatus documentStatus, int raring) {
                    return documentStatus == status; });
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
       return FindTopDocuments (raw_query,
    		   [](int document_id, DocumentStatus status, int rating) {
    	   	   	   return status == DocumentStatus::ACTUAL; });
    }

    template <typename Key>
    vector<Document> FindTopDocuments(const string& raw_query,
                                      Key key) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, key);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 if (abs(lhs.relevance - rhs.relevance) < epsilon) {
                     return lhs.rating > rhs.rating;
                 } else {
                     return lhs.relevance > rhs.relevance;
                 }
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query,
                                                        int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return {matched_words, documents_.at(document_id).status};
    }


private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = accumulate(ratings.begin(),ratings.end(),0);
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {text, is_minus, IsStopWord(text)};
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                } else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename Key>

    vector<Document> FindAllDocuments(const Query& query, Key key) const {
        map<int, double> document_to_relevance;


        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& curr_doc = documents_.at(document_id);
                if (key(document_id, curr_doc.status, curr_doc.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto& [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
                (void)_;
            }
        }

        vector<Document> matched_documents;
        for (const auto& [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                {document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }
};


//module with ostream modification
template <typename Test>
void RunTestImpl(Test test, string name) {
    test();
    cerr << name << " passed" << endl;
}

#define RUN_TEST(func) RunTestImpl((func), #func)

template <typename key, typename value>
ostream& operator<<(ostream& out, const pair<key, value>& container) {
    out << container.first << ": " << container.second;
    return out;
}

template <typename E>
void Print(ostream& out, const E& container){
    bool is_first = true;
    for (auto element : container) {
        if (!is_first) {
            out << ", ";
        }
        is_first = false;
        out << element;
    }
}

template <typename Cont>
ostream& operator << (ostream& out, const vector<Cont>& container) {
    out << "[";
    Print(out, container);
    out << "]";
    return out;
}

template <typename A>
ostream& operator<<(ostream& out, const set<A>& container) {
    out << "{";
    Print(out, container);
    out << "}";
    return out;
}

template <typename key, typename value>
ostream& operator<<(ostream& out, const map<key, value>& container) {
    out << "{";
    Print(out, container);
    out << "}";
    return out;
}

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
    	cerr << boolalpha;
    	cerr << file << "("s << line << "): "s << func << ": "s;
    	cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
    	cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
        	cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint) {
    if (!value) {
    	cerr << file << "("s << line << "): "s << func << ": "s;
    	cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
        	cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что добавленный документ находится по поисковому запросу,
// который содержит слова из документа
void TestAddDocument() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat city"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
}

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
                    "Stop words must be excluded from documents"s);
    }
}

// Тест проверяет, что документы, содержащие минус-слова из поискового запроса,
// не включаются в результаты поиска.
void TestDocumentWithMinusWordsExcluded() {
    const int doc_id1 = 42;
    const int doc_id2 = 12;
    const string document1 = "cat in the city"s;
    const string document2 = "bat over the city"s;
    const vector<int> ratings1 = {1, 2, 3};
    const vector<int> ratings2 = {4, 5, 6};
    {
        SearchServer server;
        server.AddDocument(doc_id1, document1, DocumentStatus::ACTUAL, ratings1);
        server.AddDocument(doc_id2, document2, DocumentStatus::ACTUAL, ratings2);
        const auto found_docs = server.FindTopDocuments("city -bat"s);
        ASSERT_HINT(found_docs[0].id != doc_id2 && found_docs[1].id != doc_id2, "second document should not be found"s);
    }

    {
        SearchServer server;
        server.AddDocument(doc_id1, document1, DocumentStatus::ACTUAL, ratings1);
        server.AddDocument(doc_id2, document2, DocumentStatus::ACTUAL, ratings2);
        const auto found_docs = server.FindTopDocuments("the -city"s);
        ASSERT_HINT(found_docs.empty(), "none of documents should not be found"s);
    }
}

// Тест проверяет, что документы, содержащие минус-слова из поискового запроса,
// не включаются в результаты поиска.
void TestExcludeMatching() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        auto [v, ds] = server.MatchDocument("bat in the night city"s, doc_id);
        (void) ds;
        ASSERT_HINT(v.size() == 3, "tree words should be found - in, the, city"s);
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        auto [v, ds] = server.MatchDocument("bat in the night -city"s, doc_id);
        (void) ds;
        ASSERT_HINT(v.empty(), "nothing should be found, minus word in content"s);
    }
}

//Тест проверяет, что возвращаемые при поиске документов результаты отсортированы в порядке убывания релевантности
void TestRelevanceSorting() {
    {
        const int doc_id_1 = 42;
        const string content_1 = "cat in the city city city"s;
        const vector<int> ratings_1 = {1, 2, 3};

        const int doc_id_2 = 43;
        const string content_2 = "bat bat in the city city city"s;
        const vector<int> ratings_2 = {1, 2, 3};

        const int doc_id_3 = 44;
        const string content_3 = "pin pin pin in the city"s;
        const vector<int> ratings_3 = {1, 2, 3};

        SearchServer server;
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);
        server.AddDocument(doc_id_3, content_3, DocumentStatus::ACTUAL, ratings_3);
        const auto found_docs = server.FindTopDocuments("cat bat pin"s);
        ASSERT(found_docs.size() == 3);
        ASSERT_EQUAL(found_docs[0].id, doc_id_3);
        ASSERT_EQUAL(found_docs[1].id, doc_id_2);
		//не уверен, что понял замечание в этом месте... Но для пущей убедительности сделал одинаковый рейтинг, чтобы сортировка в лямбда-функции проводилась точно только по релевантности
        ASSERT_EQUAL(found_docs[2].id, doc_id_1);
    }

    {
        const int doc_id_1 = 52;
        const string content_1 = "cat bat pin in the city"s;
        const vector<int> ratings_1 = {1, 2, 3};

        const int doc_id_2 = 53;
        const string content_2 = "pin bat the city city city"s;
        const vector<int> ratings_2 = {4, 5, 6};

        const int doc_id_3 = 54;
        const string content_3 = "bat in the city city city"s;
        const vector<int> ratings_3 = {2, 4, 2};

        SearchServer server;
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);
        server.AddDocument(doc_id_3, content_3, DocumentStatus::ACTUAL, ratings_3);
        const auto found_docs = server.FindTopDocuments("cat pin bat"s);
        ASSERT(found_docs.size() == 3);
        ASSERT_EQUAL(found_docs[0].id, doc_id_1);
        ASSERT_EQUAL(found_docs[1].id, doc_id_2);
        ASSERT_EQUAL(found_docs[2].id, doc_id_3);
    }
}

//Тест проверяет, что рейтинг добавленного документа равен среднему арифметическому оценок документа
void TestRating() {
    {
        const int doc_id_1 = 42;
        const string content_1 = "cat in the city city city"s;
        const vector<int> ratings_1 = {1, 2, 3};

        SearchServer server;
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        const auto found_docs = server.FindTopDocuments("cat bat pin"s);
        ASSERT(!found_docs.empty());
        ASSERT(abs(found_docs[0].rating - round((1+2+3+0.0)/3)) <= epsilon);
    }
	
	{
        const int doc_id_1 = 42;
        const string content_1 = "cat in the city city city"s;
        const vector<int> ratings_1 = {-1, -2, -3};

        SearchServer server;
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        const auto found_docs = server.FindTopDocuments("cat bat pin"s);
        ASSERT(!found_docs.empty());
        ASSERT(abs(found_docs[0].rating - round((-1-2-3+0.0)/3)) <= epsilon);
    }
	
	{
        const int doc_id_1 = 42;
        const string content_1 = "cat in the city city city"s;
        const vector<int> ratings_1 = {1, -2, -3};

        SearchServer server;
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        const auto found_docs = server.FindTopDocuments("cat bat pin"s);
        ASSERT(!found_docs.empty());
        ASSERT(abs(found_docs[0].rating - round((1-2-3+0.0)/3)) <= epsilon);
    }
}

//Тест проверяет, что работает фильтрация результатов поиска с использованием предиката, задаваемого пользователем
void TestLambdaFilter() {
    const int doc_id = 42;
    const string content = "cat in the city city city"s;
    const vector<int> ratings = {1, 2, 3};
    const DocumentStatus document_status = DocumentStatus::ACTUAL;

    {
        SearchServer server;
        server.AddDocument(doc_id, content, document_status, ratings);
        const auto found_docs = server.FindTopDocuments("cat pin bat"s, DocumentStatus::ACTUAL);
        ASSERT(!found_docs.empty());
        ASSERT_EQUAL(found_docs[0].id, doc_id);
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::BANNED, ratings);
        const auto found_docs = server.FindTopDocuments("cat pin bat"s, DocumentStatus::ACTUAL);
        ASSERT(found_docs.empty());
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, document_status, ratings);
        const auto found_docs = server.FindTopDocuments("cat pin bat"s, [] (int document_id, DocumentStatus status, int rating) {
            document_id +=1;
            if (status == DocumentStatus::ACTUAL) {document_id +=1;};
            return rating > 1;
        });
        ASSERT(!found_docs.empty());
        ASSERT_EQUAL(found_docs[0].id, doc_id);
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, document_status, ratings);
        const auto found_docs = server.FindTopDocuments("cat pin bat"s, [] (int document_id, DocumentStatus status, int rating) {
                     document_id +=1;
                     if (status == DocumentStatus::ACTUAL) {document_id +=1;};
                     return rating > 3;
        });
        ASSERT(found_docs.empty());
    }
}

//Тест проверяет, что работает поиск документов, имеющих заданный статус
void TestStatus() {
    const int doc_id1 = 42;
    const string content1 = "cat in the city"s;
    const vector<int> ratings1 = {1, 2, 3};
    const DocumentStatus document_status1 = DocumentStatus::ACTUAL;

    const int doc_id2 = 43;
    const string content2 = "bat in the city"s;
    const vector<int> ratings2 = {4, 5, 6};
    const DocumentStatus document_status2 = DocumentStatus::IRRELEVANT;

    const int doc_id3 = 44;
    const string content3 = "pin in the city"s;
    const vector<int> ratings3 = {7, 8, 9};
    const DocumentStatus document_status3 = DocumentStatus::BANNED;

    const int doc_id4 = 45;
    const string content4 = "core in the city"s;
    const vector<int> ratings4 = {10, 11, 12};
    const DocumentStatus document_status4 = DocumentStatus::REMOVED;

    {
        SearchServer server;
        server.AddDocument(doc_id1, content1, document_status1, ratings1);
        server.AddDocument(doc_id2, content2, document_status2, ratings2);
        server.AddDocument(doc_id3, content3, document_status3, ratings3);
        server.AddDocument(doc_id4, content4, document_status4, ratings4);
        const auto found_docs_actual = server.FindTopDocuments("bat pin core"s, DocumentStatus::ACTUAL);
        ASSERT(found_docs_actual.empty());
        const auto found_docs_irrelevant = server.FindTopDocuments("cat pin core"s, DocumentStatus::IRRELEVANT);
        ASSERT(found_docs_irrelevant.empty());
        const auto found_docs_banned = server.FindTopDocuments("cat bat core"s, DocumentStatus::BANNED);
        ASSERT(found_docs_banned.empty());
        const auto found_docs_removed = server.FindTopDocuments("cat pin bat"s, DocumentStatus::REMOVED);
        ASSERT(found_docs_removed.empty());
    }

    {
        SearchServer server;
        server.AddDocument(doc_id1, content1, document_status1, ratings1);
        server.AddDocument(doc_id2, content2, document_status2, ratings2);
        server.AddDocument(doc_id3, content3, document_status3, ratings3);
        server.AddDocument(doc_id4, content4, document_status4, ratings4);
        const auto found_docs_actual = server.FindTopDocuments("cat pin bat core"s, DocumentStatus::ACTUAL);
        ASSERT(!found_docs_actual.empty());
        ASSERT_EQUAL(found_docs_actual[0].id, doc_id1);

        const auto found_docs_irrelevant = server.FindTopDocuments("cat pin bat core"s, DocumentStatus::IRRELEVANT);
        ASSERT(!found_docs_irrelevant.empty());
        ASSERT_EQUAL(found_docs_irrelevant[0].id, doc_id2);

        const auto found_docs_banned = server.FindTopDocuments("cat pin bat core"s, DocumentStatus::BANNED);
        ASSERT(!found_docs_banned.empty());
        ASSERT_EQUAL(found_docs_banned[0].id, doc_id3);

        const auto found_docs_removed = server.FindTopDocuments("cat pin bat core"s, DocumentStatus::REMOVED);
        ASSERT(!found_docs_removed.empty());
        ASSERT_EQUAL(found_docs_removed[0].id, doc_id4);
    }
}

//Тест проверяет, что работает корректное вычисление релевантности найденных документов
void TestRelevanceAccuracy() {
    {
        const int doc_id_1 = 42;
        const string content_1 = "cat in the city city city"s;
        const vector<int> ratings_1 = {1, 2, 3};

        const int doc_id_2 = 43;
        const string content_2 = "fox fox the city city city"s;
        const vector<int> ratings_2 = {4, 5, 6};

        const int doc_id_3 = 44;
        const string content_3 = "dog dog dog in the city"s;
        const vector<int> ratings_3 = {2, 4, 2};

        SearchServer server;
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);
        server.AddDocument(doc_id_3, content_3, DocumentStatus::ACTUAL, ratings_3);
        const auto found_docs = server.FindTopDocuments("cat fox dog"s);
        ASSERT(found_docs.size() == 3);
        ASSERT_EQUAL(found_docs[0].id, doc_id_3);
        ASSERT(abs(found_docs[0].relevance - log(3)/2) < epsilon);
        ASSERT_EQUAL(found_docs[1].id, doc_id_2);
        ASSERT(abs(found_docs[1].relevance - log(3)/3) < epsilon);
        ASSERT_EQUAL(found_docs[2].id, doc_id_1);
        ASSERT(abs(found_docs[2].relevance - log(3)/6) < epsilon);
    }

    {
        const int doc_id_1 = 52;
        const string content_1 = "cat fox dog in the city"s;
        const vector<int> ratings_1 = {1, 2, 3};

        const int doc_id_2 = 53;
        const string content_2 = "fox dog the city city city"s;
        const vector<int> ratings_2 = {4, 5, 6};

        const int doc_id_3 = 54;
        const string content_3 = "dog in the city city city"s;
        const vector<int> ratings_3 = {2, 4, 2};

        SearchServer server;
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);
        server.AddDocument(doc_id_3, content_3, DocumentStatus::ACTUAL, ratings_3);
        const auto found_docs = server.FindTopDocuments("cat fox dog"s);
        ASSERT(found_docs.size() == 3);
        ASSERT_EQUAL(found_docs[0].id, doc_id_1);
        ASSERT(abs(found_docs[0].relevance - 0.25068) < epsilon);
        ASSERT_EQUAL(found_docs[1].id, doc_id_2);
        ASSERT(abs(found_docs[1].relevance - 0.0675775) < epsilon);
        ASSERT_EQUAL(found_docs[2].id, doc_id_3);
        ASSERT(abs(found_docs[2].relevance - 0) < epsilon);
    }
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestAddDocument);
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestDocumentWithMinusWordsExcluded);
    RUN_TEST(TestExcludeMatching);
    RUN_TEST(TestRelevanceSorting);
    RUN_TEST(TestRating);
    RUN_TEST(TestLambdaFilter);
    RUN_TEST(TestStatus);
    RUN_TEST(TestRelevanceAccuracy);
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    cout << "Search server testing finished"s << endl;
}
