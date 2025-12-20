#include <fstream>
#include <iomanip>

int main(int argc, char** argv) {
  if (argc != 4)
    return -1;

  const char* varName = argv[1];
  const char* inputFile = argv[2];
  const char* outputFile = argv[3];

  std::ifstream in(inputFile, std::ios::binary);

  std::ofstream out(outputFile);

  out << "#pragma once\n\n";

  out << "constexpr unsigned char " << varName << "[] = {";

  for (size_t i = 0; in.good(); ++i) {
    char byte = 0;
    in.read(&byte, 1);
    if (!in.good())
      break;

    unsigned char ub = static_cast<unsigned char>(byte);

    if (i % 12 == 0) {
      out << "\n    ";
    }
    out << "0x" << std::hex << std::setw(2) << std::setfill('0')
        << static_cast<int>(ub);
    if (in.peek() != EOF) {
      out << ", ";
    }
  }

  out << "\n};\n";

  out << "constexpr size_t " << varName << "_size = sizeof(" << varName
      << ");\n";

  return 0;
}
