#include <cstdio>
#include <string>

extern int yyparse();
extern FILE *yyin;

std::string outfilename;

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Use: %s <program.emj> <program.o>\n", argv[0]);
    return 1;
  }

  const char *input_file = argv[1];
  yyin = fopen(input_file, "r");
  if (!yyin) {
    printf("Could not open file: %s\n", input_file);
    return 1;
  }

  if (argc >= 3) {
    outfilename = argv[2];
  } else {
    outfilename = "program.o"; // default
  }

  yyparse();

  if (yyin) fclose(yyin);

  return 0;
}

