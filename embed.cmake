set(resource_file_name ${CMAKE_ARGV3})
set(output_file_name ${CMAKE_ARGV4})
set(variable_name ${CMAKE_ARGV5})

file(READ "${resource_file_name}" hex_content HEX)

string(REPEAT "[0-9a-f]" 32 pattern)
string(REGEX REPLACE "(${pattern})" "\\1\n" content "${hex_content}")

string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1, " content "${content}")

string(REGEX REPLACE ", $" "" content "${content}")

set(array_definition "static const unsigned char ${variable_name}[] =\n{\n${content}\n};")

set(output "// Auto generated file.\n${array_definition}\n")

file(WRITE "${output_file_name}" "${output}")
