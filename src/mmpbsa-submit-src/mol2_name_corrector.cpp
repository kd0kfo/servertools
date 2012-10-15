#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <cctype>
#include <map>
#include <string>

void fix_atom_names(FILE *input, FILE *output)
{
  using std::string;
  
  std::map<string,unsigned int> max_atom_index;// contains a list of all atoms and the maximum index used.
  long first_atom_line, next_section_line;
  static const size_t buffer_size = 512;
  size_t amount_read,char_size = sizeof(char);
  char buffer[buffer_size], *name_loc;
  string atom_name;
  unsigned int atom_number;
  
  if(feof(input))
    return;
  
  first_atom_line = ftell(input);
  
  while(!feof(input))
    {
      atom_name = "";
      atom_number = 0;
      next_section_line = ftell(input);
      if(fgets(buffer,buffer_size,input) == NULL)// Read line
	{
	  if(ferror(input))
	    {
	      fprintf(stderr,"fix_atom_names: Error reading input file.\nMessage: %s\n",strerror(errno));
	      exit(errno);
	    }
	  continue;
	}
      
      if(buffer[0] == 0)// empty line?
	continue;
      
      if(buffer[0] == '@')
	break;// new section

      // Skip initial whitespace and atom id
      name_loc = buffer;
      while(!isdigit(*name_loc))
	{
	  if(*name_loc == 0)
	    {
	      fprintf(stderr,"fix_atom_names: Broken atom line (%s).\nMissing atom id.",buffer);
	      exit(-1);
	    }
	  name_loc++;
	}
      
      // Get first character of atom name
      while(!isalpha(*name_loc))
	{
	  if(*name_loc == 0)
	    {
	      fprintf(stderr,"fix_atom_names: Broken atom line (%s).\nMissing atom name.",buffer);
	      exit(-1);
	    }
	  name_loc++;
	}

      // Get atom name
      while(isalpha(*name_loc))
	{
	  if(*name_loc == 0)
	    {
	      fprintf(stderr,"fix_atom_names: Broken atom line (%s).\nLine terminated after atom name.",buffer);
	      exit(-1);
	    }
	  atom_name += *name_loc;
	  name_loc++;
	}
      if(*name_loc == ' ')
	continue;// Atom name has no number.
      
      while(isdigit(*name_loc))
	{
	  atom_number *= 10;
	  atom_number += *name_loc - 0x30;// convert ascii to decimal and add
	  name_loc++;
	}
      if(max_atom_index.find(atom_name) == max_atom_index.end())
	max_atom_index[atom_name] = atom_number;
      else if(max_atom_index[atom_name] < atom_number)
	max_atom_index[atom_name] = atom_number;
    }

  // seek back to beginning of section and rewrite.
  fseek(input,first_atom_line,SEEK_SET);

  // Make a second pass; this time copy the atom lines and 
  // update the atom names as necessary.
  while(!feof(input))
    {
      atom_name = "";
      if(fgets(buffer,buffer_size,input) == NULL)// Read line
	{
	  if(ferror(input))
	    {
	      fprintf(stderr,"fix_atom_names: Error reading input file.\nMessage: %s\n",strerror(errno));
	      exit(errno);
	    }
	  continue;
	}
      
      if(buffer[0] == '@')
	break;// new section
      if(buffer[0] == 0)
	continue;// empty line

      // Get first character of atom name
      name_loc = buffer;
      while(!isalpha(*name_loc))
	{
	  if(*name_loc == 0)
	    {
	      fprintf(stderr,"fix_atom_names: Broken atom line (%s).\nMissing atom name.",buffer);
	      exit(-1);
	    }
	  name_loc++;
	}
      
      // Get atom name
      while(isalpha(*name_loc))
	{
	  atom_name += *name_loc;
	  name_loc++;
	}

      fwrite(buffer,char_size,name_loc - &buffer[0],output);

      if(!isdigit(*name_loc))
	{
	  if(max_atom_index.find(atom_name) == max_atom_index.end())
	    max_atom_index[atom_name] = 0;
	  fprintf(output,"%u",++max_atom_index[atom_name]);
	}
      fwrite(name_loc,char_size,strlen(name_loc), output);
    }
  
  fseek(input,next_section_line,SEEK_SET);
}

void rewrite_mol2(FILE *input, FILE *output)
{
  static const size_t buffer_size = 512;
  ssize_t amount_read,char_size = sizeof(char);
  char buffer[buffer_size];
  
  static const char section_name[] = "@<TRIPOS>ATOM";
  static const size_t section_name_size = strlen(section_name);

  while(!feof(input))// iterate through lines of MOL2 file.
    {
      if(fgets(buffer,buffer_size-1,input) == NULL)
	{
	  if(ferror(input))
	    {
	      fprintf(stderr,"rewrite_mol2: Error reading input file.\nMessage: %s\n",strerror(errno));
	      exit(errno);
	    }
	  continue;
	}
      amount_read = strnlen(buffer,buffer_size);
      if(buffer[0] != '@' || strncmp(buffer,section_name,section_name_size) != 0)
	{
	  // In this case, the line does not start an atom section.
	  if(amount_read != fwrite(buffer,char_size,amount_read,output))
	    {
	      if(ferror(output))
		{
		  fprintf(stderr,"rewrite_mol2: Error writing to output file.\nMessage: %s\n",strerror(errno));
		  exit(errno);
		}		  
	    }
	}
      else
	{
	  if(amount_read != fwrite(buffer,char_size,amount_read,output))
	    {
	      if(ferror(output))
		{
		  fprintf(stderr,"rewrite_mol2: Error writing to output file.\nMessage: %s\n",strerror(errno));
		  exit(errno);
		}		  
	    }
	  // In this case, the line *does* start an atom section.
	  // Fork to code that will fix atom names.
	  fix_atom_names(input,output);
	}
    }      
}

int main(int argc, char **argv)
{
  FILE *input = stdin, *output = stdout;

  if(argc == 1 || strncmp(argv[1],"--help",6) == 0)
    {
      printf("mol2_name_corrector -- Adds missing numbers from atom names in a MOL2 file.\n");
      printf("Usage: mol2_name_corrector [input file] [output file]\n");
      return 0;
    }

  if(argc >= 2)
    {
      input = fopen(argv[1],"r");
      if(input == NULL)
	{
	  fprintf(stderr,"Error opening input file (%s)\nError: %s\n", argv[1], strerror(errno));
	  return errno;
	}
    }
  
  if(argc == 3)
    {
      output = fopen(argv[2],"w");
      if(output == NULL)
	{
	  fprintf(stderr,"Error opening output file (%s)\nError: %s\n", argv[2], strerror(errno));
	  return errno;
	}
    }

  rewrite_mol2(input,output);
  
  return 0;
}
