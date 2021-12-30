#ifndef OPTPARSE_HPP
#define OPTPARSE_HPP

#include <ctime>
#include <cassert>
#include <cstring>
#include <string>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <map>
#include <algorithm>

class optparse	// add a method to return only a const ref to the map 'parameters'
{
	/// Types

	typedef struct {
		size_t nargs;
		std::string default_value;
		std::string	description;
		bool user_option;
	} parameters;

	/// variables

	std::string program_name;

	std::map<std::string, parameters> options;
	std::map<std::string, std::string> values;

public:

	enum action_t { store_true = 0, store_false = 1 };

	optparse(); /// Constructor

	void insert_option(std::string name, size_t nargs = 1, std::string description = "", std::string default_value = "");

	void insert_option_boolean(std::string name, action_t action, std::string description = "");

	auto parse(const int argc, char* const* const argv) -> int;

	template <typename T, size_t n=0>
	T
	retrieve(std::string name) const;

	template <typename T, typename U>
	std::pair<T, U>
	retrieve(std::string name) const;

	auto dump(std::string pathname) const;

private:

	void insert_option_impl_(std::string name, parameters const& p);

	auto load(std::string pathname) -> std::map<std::string, std::string>;

	auto usage(std::string error_message = "") const -> int;
};

optparse::optparse()
{
	insert_option_impl_("help", ((parameters){ .nargs = 0, .description = "Print this message", .user_option = false }));
	insert_option_impl_("load", ((parameters){ .nargs = 1, .description = "Load settings from configuration file", .user_option = false }));
}

void
optparse::insert_option(std::string name, size_t nargs, std::string description, std::string default_value)
{
	auto option_parameters = parameters {
		.nargs = nargs,
		.default_value = default_value,
		.description = description,
		.user_option = true
	};

	insert_option_impl_(name, option_parameters);
}

void
optparse::insert_option_boolean(std::string name, action_t action, std::string description)
{
	insert_option(name, 0, description, (action == store_true) ? "0" : "1");
}

auto
optparse::parse(const int argc, char* const* const argv) -> int
{
	auto ierr = int {0};

	program_name = std::string(argv[0]);

	/// Try to process all the arguments

	try
	{
		/// Loop over argv[], ignoring the first argument

		for (int i = 1; i < argc; ++i)
		{
			auto idx = strspn(argv[i], "-");

			auto key = std::string(&argv[i][idx]);

			if (!idx)
				throw std::invalid_argument("optparse::parse: argument options must start with a single/double dash"); // invalid argument

			if (!key.compare("help"))
			{
				ierr = usage();
				break;
			}

			//~ there's an argument option and it isn't --help

			if (auto option = options.find(key); option == options.end())
				throw std::invalid_argument("optparse::parse: unknow argument: " + key);

			else
			{
				auto value = std::string {};

				if (option->second.nargs == 0)
					value = std::to_string(option->second.default_value.compare("0") == 0);	// invert bool "0" -> 1

				else if (argc - i < (int) option->second.nargs +1)
					throw std::runtime_error("optparse::parse: insufficient number of argument values");

				else // ok, there's enough argument values, process all of them
				{
					value = std::string(argv[++i]);

					for (int j = 1; j < (int) option->second.nargs; ++j)
						value += ", " + std::string(argv[++i]);
				}

				if (const auto &[it, inserted] = values.try_emplace(key, value); !inserted)
					throw std::runtime_error("optparse::parse: duplicate option passed by command line: " + key);
			}
		}

		if (ierr == 0x00)
		{
			/// Read and transfer option values from the configuration file

			if (auto value = values.find("load"); value != values.end())
			{
				values.merge(load(value->second));
				values.erase("load");
			}

			/// Post processing -- check for every option besides load and help

			for (auto const& option: options)
			{
				if (!option.first.compare("load") || !option.first.compare("help"))
					continue;

				if (auto value = values.find(option.first); value == values.end() && !option.second.default_value.length())
					throw std::invalid_argument("optparse::parse: missing argument(s), e.g., " + option.first);
			}
		}
	}
	catch (const std::exception& e)
	{
		ierr = usage(e.what());
	}
	return ierr;
}

template <typename T, size_t n = 0>
T
optparse::retrieve(std::string name) const
{
	auto value = T {};

	/// Lambda function to split the option values' string

	static auto const split = [](std::string s, int pos)
	{
		auto [start, end] = std::pair(0U, s.find(","));

		for (int i = 0; end != std::string::npos; ++i)
	   	{
			if (i == pos)
				return s.substr(start, end - start);

			start = end + 1;
   		    end = s.find(",", start);
	   	}
		return s.substr(start, end);
	};

	/// Search the name in the options

	if (auto option = values.find(name); option != values.end())
	{
		auto argument = split(option->second, n);

		if (!(std::stringstream(argument) >> value))
			throw std::runtime_error("Invalid conversion of the argument '" + argument + "' to type " + typeid(T).name());
	}
	else if (auto option = options.find(name); option != options.end() && option->second.default_value.length())
	{
		auto argument = option->second.nargs == 0 ?
			std::to_string(option->second.default_value.compare("0") != 0) :
			split(option->second.default_value, n);

		if (!(std::stringstream(argument) >> value))
			throw std::runtime_error("Invalid conversion of the argument '" + argument + "' to type " + typeid(T).name());
	}
	else
		throw std::invalid_argument("optparse::retrieve no argument has been passed to option: " + name);

	return value;
}

template <typename T, typename U>
std::pair<T, U>
optparse::retrieve(std::string name) const
{
	return std::pair(retrieve<T, 0>(name), retrieve<U, 1>(name));
}

auto
optparse::dump(std::string pathname) const
{
	std::ofstream config(pathname, std::ios::app);

	if (!config.is_open())
		throw std::runtime_error("optparse::parse: opening file '" + pathname + \
				"' failed, it either doesn't exist or is not accessible.");

	std::time_t result = std::time(nullptr);

	char mbstr[100];

	///	writes standard date and time string, e.g. Sun Oct 17 04:41:13 2010 (locale dependent)

	if (std::strftime(mbstr, sizeof(mbstr), "%c", std::localtime(&result)))
	{
		config << "\n# Created automaticaly by optparse on " << std::string(mbstr) << '\n' << std::endl;
    }
	else
		config << "\n# Created automaticaly by optparse" << '\n' << std::endl;

	for (auto const& option: this->options)
	{
		auto key = option.first;

		if (auto value = values.find(key); value != values.end())
			config << key << ": " << value->second << '\n';

		else if (auto default_value = option.second.default_value; option.second.user_option)
			config << key << ": " << default_value << '\n';
	}
	config << std::endl;
	config.close();
}

// private methods

void
optparse::insert_option_impl_(std::string name, parameters const& p)
{

	if (const auto &[it, inserted] = options.try_emplace(name, p); !inserted)
		throw std::invalid_argument("optparse::insert_option: option already exists: " + name);
}


auto
optparse::load(std::string pathname) -> std::map<std::string, std::string>
{
	auto values = std::map<std::string, std::string> {};

	std::ifstream config(pathname);

	if (!config.is_open())
		throw std::runtime_error("optparse::parse: opening file '" + pathname + \
				"' failed, it either doesn't exist or is not accessible.");

	for (std::string line; std::getline(config, line); )
	{
		line.erase(std::remove_if(line.begin(), line.end(), isspace), line.end());

		if (line[0] == '#' || line.empty())
			continue;

		auto delimiterPos = line.find(":");

		auto key = line.substr(0, delimiterPos);
		auto value = line.substr(delimiterPos +1);

		if (auto option = options.find(key); option == options.end())
			throw std::runtime_error("optparse::parse: read an unexpected option from the configuration file: " + key);

		if (const auto &[it, inserted] = values.try_emplace(key, value); !inserted)
			throw std::runtime_error("optparse::parse: duplicate option found in the configuration file: " + key);

	}
	config.close();

	return values;
}

auto
optparse::usage(std::string error_message) const -> int
{
	std::clog << "Usage: " << program_name << " [OPTIONS]\n\nWhere OPTIONS are:\n";

	//! user options are defined externally, as opposed to pre-defined options.
	//	show user options LAST

	for (int user_option = 0; user_option < 2; ++user_option)
	{
		for (auto const& option: options)
		{
			if (static_cast<int>(option.second.user_option)^user_option)
				continue;

			std::clog << std::right << std::setw(16) << "--" + option.first;

			for (int index = 0; index < (int) option.second.nargs; ++index)
				std::clog << " <arg>";

			std::clog << std::right << std::setw(18 - 6 * option.second.nargs) << " ";
			std::clog << (option.second.description.length() ? option.second.description : "*** description unavailable ***") << '\n';
		}
	}
	std::clog << std::endl;

	if (error_message.length())
	{
		std::cerr << "terminate called after throwing an exception" << '\n'
			<< "  what: " << error_message << std::endl;

		return -1;
	}
	else
		return 1;
}

#endif
