#include<iostream>
#include<string>
#include<chrono>
#include<vector>
#include<iomanip>
#include<random>
#include<utility>

#include"cache_system.hpp"
#include"RLfuCache.hpp"
#include"RLruCache.hpp"
#include"RLru-kCache.hpp"
#include"RHashLruCache.hpp"
#include"RArcCache/RArcCache.hpp"
#include"RFIFOCaChe.hpp"

enum class OperationType{
	Put,
	Get
};

struct Operation{
	OperationType type;
	int key;
	int version;
};

struct Scenario{
	std::string name;
	int capacity;
	int history_capacity;
	int aging_threshold;
	std::vector<Operation> operations;
};

struct TestResult{
	std::string cache_name;
	std::size_t operation_count;
	std::size_t get_count;
	std::size_t hit_count;
	double elapsed_ms;
	double throughput;
};

std::string make_value(int key,int version){
	return "value_"+std::to_string(key)+"_"+std::to_string(version);
}

void append_warmup(
	std::vector<Operation>& operations,
	int key_count){
	for(int key=0;key<key_count;++key){
		operations.push_back({
			OperationType::Put,
			key,
			0
		});
	}
}

template<typename Cache>
TestResult run_cache(
	const std::string& cache_name,
	Cache& cache,
	const std::vector<Operation>& operations){
	std::size_t get_count=0;
	std::size_t hit_count=0;
	std::string value;

	auto start_time=std::chrono::steady_clock::now();

	for(const auto& operation:operations){
		if(operation.type==OperationType::Put){
			cache.put(
				operation.key,
				make_value(operation.key,operation.version)
			);
			continue;
		}

		++get_count;

		if(cache.get(operation.key,value)){
			++hit_count;
		}
	}

	auto end_time=std::chrono::steady_clock::now();

	double elapsed_seconds=
		std::chrono::duration<double>(
			end_time-start_time
		).count();

	double elapsed_ms=elapsed_seconds*1000.0;

	double throughput=elapsed_seconds>0.0
		?static_cast<double>(operations.size())/
			elapsed_seconds
		:0.0;

	return {
		cache_name,
		operations.size(),
		get_count,
		hit_count,
		elapsed_ms,
		throughput
	};
}

Scenario build_hot_data_scenario(){
	const int capacity=20;
	const int operation_count=500000;
	const int hot_key_count=20;
	const int cold_key_count=5000;

	std::mt19937 generator(20260904);

	std::bernoulli_distribution put_distribution(0.30);
	std::bernoulli_distribution hot_distribution(0.70);

	std::uniform_int_distribution<int> hot_key_generator(
		0,
		hot_key_count-1
	);

	std::uniform_int_distribution<int> cold_key_generator(
		hot_key_count,
		hot_key_count+cold_key_count-1
	);

	std::vector<Operation> operations;

	operations.reserve(
		static_cast<std::size_t>(
			operation_count+hot_key_count
		)
	);

	append_warmup(operations,hot_key_count);

	for(int index=0;index<operation_count;++index){
		int key=hot_distribution(generator)
			?hot_key_generator(generator)
			:cold_key_generator(generator);

		OperationType type=put_distribution(generator)
			?OperationType::Put
			:OperationType::Get;

		operations.push_back({
			type,
			key,
			index
		});
	}

	return {
		"热点数据访问",
		capacity,
		hot_key_count+cold_key_count,
		20000,
		std::move(operations)
	};
}

Scenario build_loop_scan_scenario(){
	const int capacity=50;
	const int loop_size=500;
	const int operation_count=200000;

	std::mt19937 generator(20260905);

	std::bernoulli_distribution put_distribution(0.20);

	std::uniform_int_distribution<int> random_key_generator(
		0,
		loop_size-1
	);

	std::uniform_int_distribution<int> outside_key_generator(
		loop_size,
		loop_size*2-1
	);

	std::vector<Operation> operations;

	operations.reserve(
		static_cast<std::size_t>(
			operation_count+loop_size/5
		)
	);

	append_warmup(operations,loop_size/5);

	int current_position=0;

	for(int index=0;index<operation_count;++index){
		int pattern=index%100;
		int key=0;

		if(pattern<60){
			key=current_position;
			current_position=
				(current_position+1)%loop_size;
		}else if(pattern<90){
			key=random_key_generator(generator);
		}else{
			key=outside_key_generator(generator);
		}

		OperationType type=put_distribution(generator)
			?OperationType::Put
			:OperationType::Get;

		operations.push_back({
			type,
			key,
			index
		});
	}

	return {
		"循环扫描",
		capacity,
		loop_size*2,
		3000,
		std::move(operations)
	};
}

Scenario build_workload_shift_scenario(){
	const int capacity=30;
	const int operation_count=80000;
	const int phase_length=operation_count/5;

	std::mt19937 generator(20260906);

	std::uniform_int_distribution<int> probability_generator(
		0,
		99
	);

	std::uniform_int_distribution<int> hot_key_generator(
		0,
		4
	);

	std::uniform_int_distribution<int> wide_key_generator(
		0,
		399
	);

	std::uniform_int_distribution<int> local_key_generator(
		0,
		14
	);

	std::uniform_int_distribution<int> medium_key_generator(
		5,
		49
	);

	std::uniform_int_distribution<int> cold_key_generator(
		50,
		399
	);

	std::vector<Operation> operations;

	operations.reserve(
		static_cast<std::size_t>(
			operation_count+capacity
		)
	);

	append_warmup(operations,capacity);

	for(int index=0;index<operation_count;++index){
		int phase=index/phase_length;
		int put_probability=20;
		int key=0;

		switch(phase){
			case 0:{
				put_probability=15;
				key=hot_key_generator(generator);
				break;
			}

			case 1:{
				put_probability=30;
				key=wide_key_generator(generator);
				break;
			}

			case 2:{
				put_probability=10;
				key=(index-phase_length*2)%100;
				break;
			}

			case 3:{
				put_probability=25;

				int locality=(index/800)%5;

				key=locality*15+
					local_key_generator(generator);
				break;
			}

			default:{
				put_probability=20;

				int selection=probability_generator(generator);

				if(selection<40){
					key=hot_key_generator(generator);
				}else if(selection<70){
					key=medium_key_generator(generator);
				}else{
					key=cold_key_generator(generator);
				}

				break;
			}
		}

		OperationType type=
			probability_generator(generator)<put_probability
				?OperationType::Put
				:OperationType::Get;

		operations.push_back({
			type,
			key,
			phase
		});
	}

	return {
		"工作负载切换",
		capacity,
		500,
		10000,
		std::move(operations)
	};
}

void print_results(
	const Scenario& scenario,
	const std::vector<TestResult>& results){
	std::cout<<"\n=== "<<scenario.name<<" ===\n";
	std::cout<<"缓存容量: "<<scenario.capacity<<"\n";
	std::cout<<"请求总数: "<<scenario.operations.size()<<"\n\n";

	std::cout<<std::left
		<<std::setw(16)<<"策略"
		<<std::right
		<<std::setw(14)<<"命中数"
		<<std::setw(14)<<"查询数"
		<<std::setw(14)<<"命中率"
		<<std::setw(16)<<"耗时(ms)"
		<<std::setw(18)<<"吞吐量(op/s)"
		<<"\n";

	std::cout<<std::string(92,'-')<<"\n";

	for(const auto& result:results){
		double hit_rate=result.get_count>0
			?100.0*
				static_cast<double>(result.hit_count)/
				static_cast<double>(result.get_count)
			:0.0;

		std::cout<<std::left
			<<std::setw(16)<<result.cache_name
			<<std::right
			<<std::setw(14)<<result.hit_count
			<<std::setw(14)<<result.get_count
			<<std::setw(13)
			<<std::fixed
			<<std::setprecision(2)
			<<hit_rate<<"%"
			<<std::setw(16)
			<<std::setprecision(3)
			<<result.elapsed_ms
			<<std::setw(18)
			<<std::setprecision(0)
			<<result.throughput
			<<"\n";
	}
}

void run_scenario(const Scenario& scenario){
	std::vector<TestResult> results;
	results.reserve(7);

	RrCache::RLruCache<int,std::string> lru(
		scenario.capacity
	);

	results.push_back(
		run_cache(
			"LRU",
			lru,
			scenario.operations
		)
	);

	RrCache::RLfuCache<int,std::string> lfu(
		scenario.capacity
	);

	results.push_back(
		run_cache(
			"LFU",
			lfu,
			scenario.operations
		)
	);

	RrCache::RArcCache<int,std::string> arc(
		static_cast<std::size_t>(scenario.capacity),
		2
	);

	results.push_back(
		run_cache(
			"ARC",
			arc,
			scenario.operations
		)
	);

	RrCache::RLruKCache<int,std::string> lru_k(
		scenario.capacity,
		scenario.history_capacity,
		2
	);

	results.push_back(
		run_cache(
			"LRU-K",
			lru_k,
			scenario.operations
		)
	);

	RrCache::RLfuCache<int,std::string> lfu_aging(
		scenario.capacity,
		scenario.aging_threshold
	);

	results.push_back(
		run_cache(
			"LFU-Aging",
			lfu_aging,
			scenario.operations
		)
	);

	RrCache::RFIFOCache<int,std::string> fifo(
		scenario.capacity
	);

	results.push_back(
		run_cache(
			"FIFO",
			fifo,
			scenario.operations
		)
	);

	RrCache::RHashLruCache<int,std::string> hash_lru(
		static_cast<std::size_t>(scenario.capacity),
		4
	);

	results.push_back(
		run_cache(
			"Hash-LRU",
			hash_lru,
			scenario.operations
		)
	);

	print_results(scenario,results);
}

int main(){
	const std::vector<Scenario> scenarios={
		build_hot_data_scenario(),
		build_loop_scan_scenario(),
		build_workload_shift_scenario()
	};

	for(const auto& scenario:scenarios){
		run_scenario(scenario);
	}

	return 0;
}