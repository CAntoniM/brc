
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <map>
#include <memory>
#include <optional>
#include <errno.h>
#include <thread>
#include <queue>

const size_t buffer_size = 4096; // the best size for perf

struct Station {
    float min;
    float max;
    float sum;
    int count;
    std::shared_ptr<std::mutex> mtx;
};

struct Record {
    std::string name;
    float data;
};

class Reader
{
private:
    bool eof = false;
    char *overflow_buffer;
    size_t overflow;
    int len;
    int fd;
    std::mutex *mtx;

public:
    Reader()
    {
        std::cout << "Reader Alloc" << std::endl;
        overflow_buffer = (char *)calloc(0, buffer_size + 1);
        mtx = new std::mutex();
        fd = open("measurements.txt", O_RDONLY);
    }

    ~Reader()
    {
        std::cout << "Reader DeAlloc" << std::endl;
        free(overflow_buffer);
        delete mtx;
        close(fd);
    }

    void read_next(char *buffer)
    {
        mtx->lock();
        memset(buffer, 0, buffer_size);
        if (eof)
            return;
        memcpy(buffer, overflow_buffer, overflow);
        len = read(fd, buffer + overflow, buffer_size - overflow);
        eof = (len < buffer_size - overflow);
        for (overflow = 0; buffer[buffer_size - overflow] != '\n'; overflow++)
            ;
        memset(overflow_buffer, 0, buffer_size);
        memcpy(overflow_buffer, buffer + buffer_size - overflow, overflow);
        memset(buffer + buffer_size - overflow, 0, overflow);
        std::cout << "TEST 2 " << (void *)buffer << std::endl;
        mtx->unlock();
    }
};

class Catalog
{
private:
    std::map<std::string, Station> stations;
    std::mutex *_mtx;

public:
    Catalog()
    {
        std::cout << "Catalog Alloc" << std::endl;
        _mtx = new std::mutex();
    }

    ~Catalog()
    {
        std::cout << "Catalog DeAlloc" << std::endl;
        delete _mtx;
    }

    void update(std::string name, float val)
    {
        std::map<std::string, Station>::iterator point;
        point = stations.find(name);
        if (point == stations.end())
        {
            _mtx->lock();
            point = stations.find(name);
            if (point != stations.end())
            {
                _mtx->unlock();
                return this->update(name, val);
            }
            stations[name] = (Station){val, val, val, 1, std::shared_ptr<std::mutex>(new std::mutex())};
            _mtx->unlock();
        }
        else
        {
            point->second.mtx->lock();
            point->second.max = (point->second.max < val) ? val : point->second.max;
            point->second.min = (point->second.min < val) ? val : point->second.min;
            point->second.sum += val;
            point->second.count++;
            point->second.mtx->unlock();
        }
    }

    void
    print()
    {
        std::cout << "{";
        for (std::map<std::string, Station>::iterator itr = stations.begin(); itr != stations.end(); itr++)
        {
            std::cout << itr->first << "=" << std::floor(itr->second.min * 10) / 10 << "/" << std::floor(itr->second.max * 10) / 10 << "/" << std::floor((itr->second.sum / itr->second.count) * 10) / 10 << ",";
        }
        std::cout << "}" << std::endl;
    }
};

class Worker
{
public:
    Worker(Reader *reader, Catalog *catalog)
    {
        std::cout << "Worker Alloc" << std::endl;
        this->reader = reader;
        this->catalog = catalog;
        this->buffer = (char *)calloc(0, buffer_size + 1);
        std::cout << "TEST 1" << (void *)this->buffer << std::endl;
    }

    ~Worker()
    {
        std::cout << "Worker DeAlloc" << std::endl;
        if (buffer != nullptr)
        {
            free(this->buffer);
        }
    }
    void operator()()
    {
        char data;
        char *ptr;
        float val;
        char *start;
        Record *rec;
        std::string name;
        do
        {
            reader->read_next(this->buffer);
            std::cout << "TEST 2" << (void *)this->buffer << std::endl;
            if (this->buffer != nullptr || (*(this->buffer)) == 0)
                break;
            start = this->buffer;
            for (ptr = this->buffer; *ptr != 0; ptr++)
            {
                switch (*ptr)
                {
                case '\n':
                    *ptr = 0;
                    val = atof(start);
                    start = ptr + sizeof(char);
                    catalog->update(name, val);
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
        } while (true);
    }

private:
    char *buffer = nullptr;
    Reader *reader;
    Catalog *catalog;
};

int main(int argc, char **argv)
{
    int fd = open("measurements.txt", O_RDONLY);
    Reader *reader = new Reader();
    Catalog *catalog = new Catalog();

    size_t worker_count = 4;
    Worker *workers[worker_count];
    std::thread threads[worker_count];
    for (int i = 0; i < worker_count; i++)
    {
        workers[i] = new Worker(reader, catalog);
        threads[i] = std::thread(*workers[i]);
    }

    for (int i = 0; i < worker_count; i++)
    {
        threads[i].join();
        delete workers[i];
    }

    catalog->print();

    delete reader;
    delete catalog;
}