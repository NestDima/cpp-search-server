#include "request_queue.h"

using namespace std;

RequestQueue::RequestQueue(const SearchServer& search_server)
        : search_server_(search_server)
        , requests_without_resault_(0)
        , current_time_(0){
    }
    vector<Document> RequestQueue::AddFindRequest(string_view raw_query, DocumentStatus status) {
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
    vector<Document> RequestQueue::AddFindRequest(string_view raw_query) {
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
    int RequestQueue::GetNoResultRequests() const {
        return requests_without_resault_;
    }

    void RequestQueue::RemoveOld(){
        while (!requests_.empty() && min_in_day_ <= current_time_ - requests_.front().time) {
            if (0 == requests_.front().results) {
                --requests_without_resault_;
            }
            requests_.pop_front();
        }
    }
