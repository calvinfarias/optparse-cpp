# optparse-cpp
A C++ header that implements a python-like optparse 

[![License](https://img.shields.io/badge/license-GPLv3-black.svg)](../master/LICENSE)
[![Language](https://img.shields.io/badge/language-C%2B%2B-lightgrey.svg)](https://isocpp.org/)

This is an easy-to-implement command-line option for C++ programs. Take as an example the following code, where command-line arguments will set the options for a molecular dynamics simulation.

```C++
auto opts = optparse {};
 
// parameters: name, number of arguments, description, and default value
opts.insert_option("verbose", 0, "Verbosely list observers's data", "0");
opts.insert_option("period", 2, "Set the time length of the simulation");
opts.insert_option("timestep", 1, "Set the time interval between snapshots");

if (auto ierr = opts.parse(argc, argv); ierr != 0)
    exit(ierr);
```

It is as easy as it seems. Compiling and running with the pre-defined `--help` option yields

```bash
$ ./optparse.x --help
Usage: ./optparse.x [OPTIONS]

Where OPTIONS are:
          --help                  Print this message
          --load <arg>            Load settings from configuration file
        --period <arg> <arg>      Set the time length of the simulation
      --timestep <arg>            Set the time interval between snapshots
       --verbose                  Verbosely list observers's data
```



### Setting the options

The optparse instance allows adding options via the **insert_option** method, whose parameters are respectively the option name, the number of arguments, the description, and the default value. Except for the option name, the other parameters are optional. If the parameters aren't passed, the expected number of arguments will be one, with no default value, and no description available. If the default value is set, there's no obligation to pass it in the command-line.

Besides the `--help` option, `--verbose` also doesn't accept arguments. Zero as the number of arguments is the way to inform optparse that it should be interpreted as a boolean, in this case it must have a default value ("0" or "1"). Alternatively, a boolean option can be set with the **insert_option_boolean** method, which has as parameters the name, the behavior, and the description, such as in

```C++
/*	as opposed to
opts.insert_option("verbose", 0, "Verbosely list observers's data", "0");
	it could be written as:
 */
opts.insert_option_boolean("verbose", optparse::store_true, "Verbosely list observers's data");
```

where, `optparse::store_true` behavior sets the default value to `false`, returning `true` only if `--verbose` is passed in the command-line. `optparse::store_false` sets the alternative behavior.

It's also possible to read all the user-defined options from a file using the `--load` option. The structure of the configuration file must be one `option: 1st_value [, 2nd_value, 3rd_value, ...]` per line. For example

```bash
$ cat <<-EOF > settings.txt
period: 0, 1
timestep: 0.1
verbose: 1
EOF

$ ./optparse.x --load settings.txt 
```

Of course, command-line arguments overwrite the options read from the configuration file. Therefore,

```bash
$ ./optparse.x --load settings.txt --timestep 0.01
```

will have all the options defined in the configuration file but the `timestep`, which will be set to `0.01`.



### Getting the options's values

Inside the code, the values can be obtained with the **retrieve** method, which specifies the returning type as a template parameter.

```C++
// get the command-line --verbose option as a bool
auto verbose = opts.retrieve<bool>("verbose");
```

or

```C++
// get the command-line --period option as a std::pair<double, double>
auto period = opts.retrieve<double, double>("period");
```

Alternatively, for options with more than two arguments, the values can be obtained by index, as in

```C++
// get the command-line --period 1st option as a double
// the index can be any value between 0 and the number of arguments -1
auto starting_time = opts.retrieve<double, 0>("period");
```

The arguments are all stored as `std::string`. The cast is made with `stringstream` via operator `>>`. Therefore, all the primitive types should work properly. Any casting that is a invalid conversion will throw a `std::runtime_error`.
