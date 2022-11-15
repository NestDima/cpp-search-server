#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <map>
#include <cmath>

using namespace std;

//определяет максимальное число документов, которые будут выведены
const int MAX_RESULT_DOCUMENT_COUNT = 5;

//чтение строки
string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

//чтение числа, соответствующему числу индексируемых документов
int ReadLineWithNumber() {
    int result = 0;
    cin >> result;
    ReadLine();
    return result;
}

//разделение строки на отделные слова
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

//структура, первый элемент которой - id документа, второй - его релевантность
struct Document {
    int id;
    double relevance;
};

class SearchServer {
public:
//метод, наполняющий stop_words_ стоп-словами
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

	//метод, добавляющий документы
    void AddDocument(int document_id, const string& document) {
		//разобъём строку-документ на слова, исключая стоп-слова
        const vector<string> words = SplitIntoWordsNoStop(document);
		//заполняем word_to_documents_freqs_ словами из запроса, привязывая к ним id и tf
        for(const string& word : words){
            word_to_documents_freqs_[word][document_id] += 1.0/words.size();
        }
        ++document_count_;
    }

	//метод, ищущий документы по запросу и сортирующий по релевантности
    vector<Document> FindTopDocuments(const string& raw_query) const {
        const Query query_words = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query_words);
		
		//сортировка по убыванию релевантности запроса
        sort(matched_documents.begin(), matched_documents.end(),
			//лямбда-функция компаратора
             [](const Document& lhs, const Document& rhs) {
                 return lhs.relevance > rhs.relevance;
             });
		//ограничиваем выдачу 5-ю документами
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

private:
	//структура, в которую будем заносить плюс и минус слова из запроса
    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };
	
	//хранит в себе слово, соответствующие ему id документа (из которгого оно взято)  и tf (соответственно)
    map<string, map<int,double>> word_to_documents_freqs_;
	//считаем число документов в эту переменную
    int document_count_ = 0;
	//сет из стоп-слов
    set<string> stop_words_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }
	
	//метод, разбивающий запрос на слова, не включая стоп-слов
    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }
	//парсинг запроса - разбиваем запрос на слова, разбиваем на плюс и минус слова, на выходе структура Query
    Query ParseQuery(const string& text) const {
        Query query_words;
        for (const string& word : SplitIntoWordsNoStop(text)) {
            if (word[0] == '-'){
                query_words.minus_words.insert(word.substr(1));
            }
            else{
                query_words.plus_words.insert(word);
            }
            
        }
        return query_words;
    }

	//метод, который ищет документы, соответствующие запросу
    vector<Document> FindAllDocuments(const Query& query_words) const {
		//возвращаемая переменная
        vector<Document> matched_documents;
		//переменная, в которую загружают id и вычисленную для него релевантность
        map<int,double> document_to_relevance;
		//пробегаем по всем плюс словам запроса
		for(const auto& word : query_words.plus_words){
			//если слово не "" и содержится в документах
			if((!word.empty()) && (!(word_to_documents_freqs_.count(word) == 0))){
				//считаем idf
				double idf = log(document_count_/(word_to_documents_freqs_.at(word).size()*1.0));
				//для каждого плюс-слова, которое есть в документах, считаем релевантность
				for (const auto& [id,tf] : word_to_documents_freqs_.at(word)){
					document_to_relevance[id] += idf*tf;
				}
			}
		}
		//теперь проходимся по всем минус-словам
		for(const auto& word : query_words.minus_words){
			//и если они есть в документах
			if(!word.empty() && (!(word_to_documents_freqs_.count(word) == 0))){
				for(const auto& [id,tf] : word_to_documents_freqs_.at(word)){
					//удаляем id Документов из списка релевантных
					document_to_relevance.erase(id);
				}
			}
		}
        for(const auto& [ids,relevance]:document_to_relevance){
            matched_documents.push_back({ids, relevance});
        }
        return matched_documents;
    }
};

SearchServer CreateSearchServer() {
    SearchServer search_server;
    search_server.SetStopWords(ReadLine());

    const int document_count = ReadLineWithNumber();
    for (int document_id = 0; document_id < document_count; ++document_id) {
        search_server.AddDocument(document_id, ReadLine());
    }

    return search_server;
}

int main() {
    const SearchServer search_server = CreateSearchServer();
    const string query = ReadLine();
    for (const auto& [document_id, relevance] : search_server.FindTopDocuments(query)) {
        cout << "{ document_id = "s << document_id << ", "
             << "relevance = "s << relevance << " }"s << endl;
    }
}