/*
Здравствуйте, Марина!

Спасибо за комментарии к работе!
Идея сделать шаблонным политику выполнения замечательная - код стал и понятнее, и компактнее! Здорово

А про механизм using я или забыл напрочь, или вообще не знал (сейчас уже в следующем спринте как раз в одном из примеров попалось).

Про то, что контейнер doc_ids_ снова стал вектором...
Тут действительно извиняюсь, в последнем спринте и правда был set, но я несколько запутался в версиях
(и потому долго не мог пройти тренажёр в начале спринта, вроде потом всё исправил как в последней версии должно было быть, но всё равно вот затесался вектор)
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <map>
#include <utility>
#include <execution>
#include <random>
#include <future>

#include "concurrent_map.h"
#include "document.h"
#include "string_processing.h"
#include "read_input_functions.h"
#include "log_duration.h"

const int kMaxResultDocumentCount = 5;
const double kEpsilon = 1e-6;

using MatchDocumentType = std::tuple<std::vector<std::string_view>, DocumentStatus>;

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);
    explicit SearchServer(const std::string& stop_words_text) : SearchServer(SplitIntoWords(stop_words_text)){}
    explicit SearchServer(std::string_view& stop_words_text) : SearchServer(SplitIntoWords(stop_words_text)){}

    void AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const;
    std::vector<Document> FindTopDocuments(std::string_view raw_query) const;
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status) const;

    template <typename ExecutionPolicy, typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(ExecutionPolicy, std::string_view raw_query, DocumentPredicate document_predicate) const;
    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy, std::string_view raw_query, DocumentStatus status) const;
    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy, std::string_view raw_query) const;

    int GetDocumentCount() const;

    MatchDocumentType MatchDocument(std::string_view raw_query, int document_id) const;
    MatchDocumentType MatchDocument(const std::execution::sequenced_policy&, std::string_view raw_query, int document_id) const;
    MatchDocumentType MatchDocument(const std::execution::parallel_policy&, std::string_view raw_query, int document_id) const;

    std::set<int>::iterator begin();
    std::set<int>::iterator end();

    void RemoveDocument(int document_id);
    void RemoveDocument(const std::execution::sequenced_policy&, int document_id);
    void RemoveDocument(const std::execution::parallel_policy&, int document_id);
    
   const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

private:
    struct DocumentData {
        std::string data_string_;
        int rating;
        DocumentStatus status;
    };
    std::set<std::string, std::less<>> stop_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> doc_ids_set_;
    std::map<int, std::map<std::string_view, double>> word_frequencies_;

    static bool IsValidWord(std::string_view word);

    bool IsStopWord(std::string_view word) const;

    std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(std::string_view& text) const;

    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    Query ParseQuery(std::string_view text) const;

    double ComputeWordInverseDocumentFreq(std::string_view word) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::sequenced_policy&, const Query& query, DocumentPredicate document_predicate) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::parallel_policy&, const Query& query, DocumentPredicate document_predicate) const;
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words) : stop_words_(MakeUniqueNonEmptyStrings(stop_words)){
    if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        throw std::invalid_argument("Invalid symbols or word with minus-symbols only!");
    }
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy policy, std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(policy, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
    });
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy policy, std::string_view raw_query) const {
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const {
    return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template <typename ExecutionPolicy, typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy policy, std::string_view raw_query, DocumentPredicate document_predicate) const {
    const SearchServer::Query query = SearchServer::ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(policy, query, document_predicate);
    sort(policy, matched_documents.begin(), matched_documents.end(),
        [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < kEpsilon) {
                return lhs.rating > rhs.rating;
            }
            else {
                return lhs.relevance > rhs.relevance;
            }
        });
    if (matched_documents.size() > kMaxResultDocumentCount) {
        matched_documents.resize(kMaxResultDocumentCount);
    }
        return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
    return FindAllDocuments(std::execution::seq, query, document_predicate);
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::sequenced_policy&, const Query& query, DocumentPredicate document_predicate) const {
    std::map<int, double> document_to_relevance;
    for (std::string_view word : query.plus_words){
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

    for (std::string_view word : query.minus_words){
        if (word_to_document_freqs_.count(word) == 0){
            continue;
        }
        for (const auto [document_id, sec] : word_to_document_freqs_.at(word)){
            document_to_relevance.erase(document_id);
            (void)sec;
        }
    }

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance){
        matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::parallel_policy&, const Query& query, DocumentPredicate document_predicate) const {
    ConcurrentMap<int, double> document_to_relevance(101);

    for_each(std::execution::par, query.plus_words.begin(), query.plus_words.end(),
            [this, &document_predicate, &document_to_relevance] (std::string_view word) {
                 if (word_to_document_freqs_.count(word) == 0) {
                    return;
                }
                const double document_freq = ComputeWordInverseDocumentFreq(word);
                for (const auto& [document_id, freq] : word_to_document_freqs_.at(word)) {
                    const auto& document_data = documents_.at(document_id);
                    if (document_predicate(document_id, document_data.status, document_data.rating)) {
                        document_to_relevance[document_id].value_reference += freq * document_freq;
                    }
                }
            }
    );

    for_each(std::execution::par, query.minus_words.begin(), query.minus_words.end(),
            [&](std::string_view word) {
                if (word_to_document_freqs_.count(word) == 0) {
                    return;
                }
                for (const auto& [document_id, sec] : word_to_document_freqs_.at(word)) {
                    document_to_relevance.Erase(document_id);
                    (void)sec;
                }
            }
    );

    const auto& document_to_relevance_builded = document_to_relevance.BuildMap();

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance_builded){
        matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}
