#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <mutex>
#include <filesystem>
#include <chrono>
#include <condition_variable>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

//Buffer size is 4kb - default page size for many OS
constexpr std::streamsize BUFFER_SIZE = 4096u;

struct SharedMemoryBuffer
{
	boost::interprocess::interprocess_mutex buffer_mutex;
	boost::interprocess::interprocess_condition buffer_state;
	bool data_ready = false;
	bool finished = false;
	char buffer[BUFFER_SIZE];
	std::streamsize size;

	SharedMemoryBuffer() : size(0) {}

private:
	SharedMemoryBuffer(const SharedMemoryBuffer&) = delete;
	SharedMemoryBuffer& operator = (const SharedMemoryBuffer&) = delete;
};

void ReadFromFile(std::ifstream& fileToRead, boost::interprocess::managed_shared_memory& segment)
{
#if DebugMode
	std::cout << "ReadFromFile" << std::endl;
#endif

	try {

		auto sharedBuffer = segment.find_or_construct<SharedMemoryBuffer>("sharedBuffer")();

		while (!fileToRead.eof())
		{
			boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex> lock(sharedBuffer->buffer_mutex);
			fileToRead.read(sharedBuffer->buffer, BUFFER_SIZE);
			sharedBuffer->size = fileToRead.gcount();
			sharedBuffer->data_ready = true;
			sharedBuffer->finished = fileToRead.eof();
			sharedBuffer->buffer_state.notify_one();

			if (!sharedBuffer->finished)
			{
				sharedBuffer->buffer_state.wait(lock, [&] { return !sharedBuffer->data_ready; });
			}
		}
	}
	catch (const std::exception& e) {
		std::cerr << "There is an expection in ReadFromFile(): " << e.what() << std::endl;
	}

	fileToRead.close();
}

void WriteToFile(std::ofstream& fileToWrite, boost::interprocess::managed_shared_memory& segment)
{
#if DebugMode
	std::cout << "WriteToFile" << std::endl;
#endif

	try
	{
		auto sharedBuffer = segment.find_or_construct<SharedMemoryBuffer>("sharedBuffer")();

		while (!sharedBuffer->finished)
		{
			boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex> lock(sharedBuffer->buffer_mutex);
			sharedBuffer->buffer_state.wait(lock, [&] { return sharedBuffer->data_ready || sharedBuffer->finished; });

			if (sharedBuffer->data_ready)
			{
				fileToWrite.write(sharedBuffer->buffer, sharedBuffer->size);
				sharedBuffer->data_ready = false;
				sharedBuffer->buffer_state.notify_one();
			}
		}
	}
	catch (const std::exception& e) {
		std::cerr << "There is an expection in WriteToFile(): " << e.what() << std::endl;
	}
	fileToWrite.close();
}

int main(int argc, char* argv[])
{
	//Check for arguments
	if (argc != 4)
	{
		std::cerr << "There are should be 3 arguments: <source_file_path> <destination_file_path> <shared_memory_name>" << std::endl;
		return EXIT_FAILURE;
	}

	std::string sourceFilePath = argv[1];
	std::string destinationFilePath = argv[2];
	std::string sharedMemoryName = argv[3];

	//Check if the target file exists already
	/*std::string fileToWriteExist(destinationFilePath);
	if (std::filesystem::exists(fileToWriteExist)) {
		char statusOverwrite;

		std::cout << "File " << fileToWriteExist << " exists. Overwrite? (y/n)" << std::endl;
		std::cin >> statusOverwrite;
		std::cout << std::endl;

		if ('n' == statusOverwrite)
		{
			std::cerr << "Aborted. File has not been overwritten." << std::endl;
			return EXIT_FAILURE;
		}
		else if ('n' != statusOverwrite && 'y' != statusOverwrite)
		{
			std::cerr << "'" << statusOverwrite << "' is an invalid argument. Aborted." << std::endl;
			return EXIT_FAILURE;
		}
	}*/

	boost::interprocess::managed_shared_memory segment(boost::interprocess::open_or_create, sharedMemoryName.c_str(), 65536);

	if (!segment.find<SharedMemoryBuffer>("sharedBuffer").first)
	{
		std::cout << "Acting as reader. Wait..." << std::endl;
		std::ifstream fileToRead(argv[1], std::fstream::binary);
		if (!fileToRead)
		{
			std::cerr << "File for read cannot be open!" << std::endl;
			return EXIT_FAILURE;
		}
		ReadFromFile(std::ref(fileToRead), segment);
	}
	else
	{
		std::ofstream fileToWrite(argv[2], std::ofstream::binary);
		if (!fileToWrite)
		{
			std::cerr << "File for write cannot be open!" << std::endl;
			return EXIT_FAILURE;
		}

		std::cout << "Acting as writer. Wait..." << std::endl;
		WriteToFile(std::ref(fileToWrite), segment);
		boost::interprocess::shared_memory_object::remove(sharedMemoryName.c_str());
		std::cout << "File " << argv[1] << " has been copied to " << argv[2] << std::endl;
	}

	return EXIT_SUCCESS;
}