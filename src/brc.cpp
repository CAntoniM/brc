
#include <unistd.h>
#include <fcntl.h>
#include <chrono>
#include <iostream>
#include <string>
#include <map>
#include <atomic>
#include <memory>
#include <optional>
#include <thread>
#include <queue>
#include <queue>

struct Station {
    float min;
    float max;
    float sum;
    int count;
};

struct Record {
    std::string name;
    float data;
};
template<typename t>
class Queue {
    private:
        std::queue<t> _data;
        std::mutex _mtx;
    public:
        void push(t val) {
            _mtx.lock();
            _data.push(val);
            _mtx.unlock();
        }
        std::optional<t> pop() {
            _mtx.lock();
            if (_data.size() <= 0) {
                return std::nullopt;
            }
            t ret_val = _data.back();
            _data.pop();
            _mtx.unlock();
            return ret_val;
        }
};

typedef Queue<char* > in_channel;
typedef Queue<Record* > out_channel;
class Worker {
    public:
        Worker() {

        }
        Worker(std::shared_ptr<in_channel> read_channel, std::shared_ptr<out_channel> write_channel) {
            this->read_channel = read_channel;
            this->write_channel = write_channel;
        }
        void operator() () {
            char data;
            char* buffer;
            char* ptr;
            float val;
            char* start;
            Record* rec;
            std::string name;
            do {
                std::optional<char*> data =  read_channel->pop();
                buffer = data.value_or(nullptr);
                if (buffer == nullptr) continue;
                std::cout << buffer << std::endl;
                if (*buffer == 0) break;
                
                for (ptr = buffer; *ptr != 0; ptr ++) {
                    switch (*ptr) {
                    case '\n': 
                        *ptr = 0;
                        val = atof(start);
                        start = ptr + sizeof(char);
                        rec = (Record*) malloc(sizeof(Record));
                        rec->name = name;
                        rec->data = val;
                        write_channel->push(rec);
                        break;
                    case ';': 
                        *ptr = 0;
                        name = start;
                        start = ptr + sizeof(char);
                       break;
                    default:
                        break;
                    }
                }
                std::cout << buffer << std::endl;
                free(buffer);
            } while(true);
            rec = (Record*)malloc(sizeof(Record));
            rec->data = 0;
            rec->name = "";
            write_channel->push(rec);
        }
    private: 
        std::shared_ptr<in_channel> read_channel;
        std::shared_ptr<out_channel> write_channel;
};

int main (int argc, char** argv) {
    const size_t buffer_size = 4096; //the best size for perf
    bool eof = false;
    char* buffer = (char*) malloc(buffer_size + 1);
    char* next_buffer;
    char* tmp;
    buffer[buffer_size] = 0;
    int len;
    int overflow = 0;
    int fd = open("measurements.txt",O_RDONLY);
    std::map<std::string, Station> stations;
    std::map<std::string, Station>::iterator point;
    std::shared_ptr<in_channel> write_channel(new in_channel());
    std::shared_ptr<out_channel> read_channel(new out_channel());

    size_t worker_count = 4;
    Worker workers[worker_count];
    std::thread threads[worker_count];
    for (int i = 0; i < worker_count; i++) {
        workers[i] = Worker(write_channel,read_channel);
        threads[i] = std::thread(workers[i]);
    }

    do {
        len = read(fd,buffer+ overflow,buffer_size - overflow);
        eof = (len < buffer_size - overflow);
        for(overflow = 0; buffer[buffer_size - 1 - overflow] != '\n'; overflow++);
        next_buffer = (char*) malloc(buffer_size);
        memcpy(next_buffer,buffer + buffer_size - overflow, overflow);
        memset(buffer + buffer_size - overflow,0,overflow);
        write_channel->push(buffer);
        buffer = next_buffer;
    }while(!eof);

    for (int i = 0; i < worker_count; i++) write_channel->push("");

    int count = 0;
    Record* rec; 
    while(true) {
        std::optional<Record*> data = read_channel->pop();
        rec = data.value_or(nullptr);
        if (!rec) continue;
        if (rec->name.empty()) {
            count++;
            if (count >= worker_count ) break;
            continue;
        }
        std::cout << std::endl;
        point = stations.find(rec->name);
        if (point == stations.end()) {
            stations.insert(std::make_pair(rec->name,(Station){rec->data,rec->data,rec->data,1}));
        } else {
            point->second.max = (point->second.max < rec->data) ? rec->data : point->second.max;
            point->second.min = (point->second.min < rec->data) ? rec->data : point->second.min;
            point->second.sum += rec->data;
            point->second.count++;
        }
        free(rec);
    }

    std::cout << "{";

    for (std::map<std::string, Station>::iterator itr = stations.begin(); itr != stations.end(); itr ++) {
        std::cout << itr->first << "=" << std::floor(itr->second.min * 10)/10 << "/" << std::floor(itr->second.max * 10)/10 << "/" << std::floor((itr->second.sum/ itr->second.count) * 10)/10 << ",";
    }
    std::cout << "}" << std::endl;
}