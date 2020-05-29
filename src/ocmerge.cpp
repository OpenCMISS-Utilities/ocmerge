/* \file
 * \author Gene Soudlenkov
 * \brief
 *
 * \section LICENSE
 *
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is OpenCMISS
 *
 * The Initial Developer of the Original Code is University of Auckland,
 * Auckland, New Zealand, the University of Oxford, Oxford, United
 * Kingdom and King's College, London, United Kingdom. Portions created
 * by the University of Auckland, the University of Oxford and King's
 * College, London are Copyright (C) 2007-2010 by the University of
 * Auckland, the University of Oxford and King's College, London.
 * All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 */

#include <stdio.h>
#include <string>
#include <vector>
#include <math.h>
#include <glob.h>

using namespace std;

//Node information
struct Node
{
	int id;
	std::vector<double> values;
};

//Element information
struct Elem
{
	std::vector<int> id;
	std::vector<double> values;
	std::vector<int> nodes;
	std::vector<double> scale;
};

typedef std::vector<std::string> STRLIST;
typedef std::vector<Node> NODELIST;
typedef std::vector<Elem> ELEMLIST;
typedef enum { UNDEF_TYPE=-1, ELEMENTS_TYPE=0, NODES_TYPE=1 } FILETYPE;
//Words that identify information structures
const char *stoppers[]={"Element","Node"};

double precision=0.0000001; //Proximity precision
bool quiet=false; //generate additional output for debugging
bool add_header=false; //output headers


//Returns true if two values are withing "precision" distance from each other
inline bool close_enough(double v1,double v2)
{
	return fabs(v1-v2)<=precision;
}

//Returns true if two containers are equal
template<class T>
bool compare(const T& l1, const T& l2)
{
	if(l1.size()!=l2.size())
		return false;
	for(int i=0;i<l1.size();i++)
		if(l1[i]!=l2[i])
			return false;
	return true;
}

//Returns true if two double-based containers are equal
//using proximity comparison
bool compare(const std::vector<double>& l1, const std::vector<double>& l2)
{
	if(l1.size()!=l2.size())
		return false;
	for(int i=0;i<l1.size();i++)
		if(!close_enough(l1[i],l2[i]))
			return false;
	return true;
}


//expand file list if file name contains globing operations
void expand_file(std::vector<std::string>& res,const std::string& pattern)
{
	glob_t glob_result;
    memset(&glob_result, 0, sizeof(glob_result));

    // do the glob operation
    int return_value = glob(pattern.c_str(), GLOB_TILDE, NULL, &glob_result);
    if(return_value) 
	{
        globfree(&glob_result);
		return;
    }

	for(size_t i = 0; i < glob_result.gl_pathc; ++i)
	{
		res.push_back(string(glob_result.gl_pathv[i]));
	}
	globfree(&glob_result);
}

//expand file list if necessary
void expand_file_list(std::vector<std::string>& lst)
{
	std::vector<std::string> res;

	for(int i=0;i<lst.size();i++)
	{
		if(lst[i].find('*')==std::string::npos && lst[i].find('?')==std::string::npos)
		{
			res.push_back(lst[i]);
		}
		else
		{
			expand_file(res,lst[i]);
		}
	}
	lst=res;
}

//Break a string into a series of substrings using delimiters
void tokenize(const std::string& str,const std::string& delimiters,std::vector<std::string>& tokens)
{
    // skip delimiters at beginning.
    std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);

    // find first "non-delimiter".
    std::string::size_type pos = str.find_first_of(delimiters, lastPos);

    while(std::string::npos != pos || std::string::npos != lastPos)
    {
        // found a token, add it to the vector.
        std::string ss=str.substr(lastPos, pos - lastPos);
        while(ss.size() && ss[0]<=0x20)
            ss=ss.substr(1);
        while(ss.size() && ss[ss.size()-1]<=0x20)
            ss=ss.substr(0,ss.size()-1);

        if(ss!="")
            tokens.push_back(ss);

        // skip delimiters.  Note the "not_of"
        lastPos = str.find_first_not_of(delimiters, pos);

        // find next "non-delimiter"
        pos = str.find_first_of(delimiters, lastPos);
    }
}


//Read an input file header until the stopper word found
bool read_header(FILE *f,const std::string& stopper,std::string& str)
{
	bool res=false;
	long offset;
	char tmp[8192];
	char *ptr;

	while((offset=ftell(f),fgets(tmp,8191,f)))
	{
		if(tmp[strlen(tmp)-1]=='\n')
			tmp[strlen(tmp)-1]=0;
		ptr=tmp;
		while(*ptr && *ptr<=0x20)
			ptr++;
		if(!*ptr)
			continue; //skip empty lines
		if(!strncmp(ptr,stopper.c_str(),stopper.size()))
		{
			fseek(f,offset,SEEK_SET);
			res=true;
			break;
		}
		str+=tmp;
		str+='\n';
	}
	return res;
}

//Compare two nodes by their ids
bool node_sort(Node& n1, Node& n2)
{
	return (n1.id < n2.id);
}


//Read node information
void read_body_node(FILE *f,NODELIST& lst)
{
	bool res=false;
	char tmp[8192];
	long offset;
	char *ptr;
	Node node;
	int state=0; //state 0 expect node id, state 1 - expect node values

	while((offset=ftell(f),fgets(tmp,8191,f)))
	{
		if(tmp[strlen(tmp)-1]=='\n')
			tmp[strlen(tmp)-1]=0;
		ptr=tmp;
		while(*ptr && *ptr<=0x20)
			ptr++;
		if(!*ptr)
			continue; //skip empty lines
		if (!state)
		{
			//Node id expected
			if (strncmp(ptr,"Node:",5))
				continue;
			node.id=atoi(ptr+6);
			state=1;
			node.values.clear();
		}
		else
		{
			char *end;
			double v=strtod(ptr,&end);
			if(end==ptr)
			{
				//revert back to state 0
				fseek(f,offset,SEEK_SET);
				state=0;
				lst.push_back(node);
				node.id=0;
				continue;
			}
			if(fabs(v)<precision)
				v=fabs(v);
			node.values.push_back(v);
		}
	}
	if(node.id)
		lst.push_back(node);
	std::sort(lst.begin(), lst.end(), node_sort);
}


bool process_nodes(const std::string& hdr,NODELIST& src, const std::string& output)
{
	FILE *fout=stdout;
	NODELIST::iterator isrc;

	if(output.size())
		fout=fopen(output.c_str(),"wt");
	if(!fout)
	{
		fprintf(stderr,"Error opening output file %s\n",output.c_str());
		return false;
	}
	if(add_header)
		fprintf(fout,"%s\n",hdr.c_str());

	for(isrc=src.begin();isrc!=src.end();++isrc)
	{
		fprintf(fout,"Node: %d\n",isrc->id);
		for(int i=0;i<isrc->values.size();i++)
			fprintf(fout,"    %lf\n",isrc->values[i]);
	}
	fclose(fout);
	return true;
}


//Sort elements using their IDs sequences
bool elem_sort(Elem& n1, Elem& n2)
{
	if(n1.id.size()<n2.id.size())
		return true;
	else if(n1.id.size()>n2.id.size())
		return false;
	for(int i=0;i<n1.id.size();i++)
		if(n1.id[i]<n2.id[i])
			return true;
		else if(n1.id[i]>n2.id[i])
			return false;
	return false;
}

void read_body_elems(FILE *f,ELEMLIST& list)
{
	bool res=false;
	char tmp[8192];
	long offset;
	char *ptr;
	Elem elem;
	int state=0; //state 0 expect element id, state 1 - values, 3 - nodes, 5 - scale
	std::vector<std::string> strs;

	while((offset=ftell(f),fgets(tmp,8191,f)))
	{
		if(tmp[strlen(tmp)-1]=='\n')
			tmp[strlen(tmp)-1]=0;
		ptr=tmp;
		while(*ptr && *ptr<=0x20)
			ptr++;
		if(!*ptr)
			continue; //skip empty lines
		switch(state)
		{
			case 0:
			{
				//Element id expected
				if (strncmp(ptr,"Element:",8))
					continue;

				elem.id.clear();
				strs.clear();
				tokenize(ptr+8," \t",strs);
				for(int i=0;i<strs.size();i++)
					elem.id.push_back(atoi(strs[i].c_str()));
				state=1;
				elem.values.clear();
				elem.nodes.clear();
				elem.scale.clear();
			}
			break;
			case 1:
			{
				//Values expected
				if (strcmp(ptr,"Values:"))
				{
					state=0; //skip the rest
					fseek(f,offset,SEEK_SET);
					continue;
				}
				state++;
			}
			break;
			case 2: //Values body
			{
				if(!isdigit(*ptr) && *ptr!='-')
				{
					state++;
					fseek(f,offset,SEEK_SET);
					continue;
				}
				strs.clear();
				tokenize(ptr," \t",strs);
				for(int i=0;i<strs.size();i++)
				{
					double v=strtod(strs[i].c_str(),NULL);
					if(fabs(v)<precision)
						v=fabs(v);
					elem.values.push_back(v);
				}
			}
			break;
			case 3:
			{
				//Nodes expected
				if (strcmp(ptr,"Nodes:"))
				{
					state=0; //skip the rest
					fseek(f,offset,SEEK_SET);
					continue;
				}
				state++;
			}
			break;
			case 4: //Nodes body
			{
				if(!isdigit(*ptr) && *ptr!='-')
				{
					state++;
					fseek(f,offset,SEEK_SET);
					continue;
				}
				strs.clear();
				tokenize(ptr," \t",strs);
				for(int i=0;i<strs.size();i++)
					elem.nodes.push_back(strtol(strs[i].c_str(),NULL,10));
			}
			break;
			case 5:
			{
				//Scale factors expected
				if (strcmp(ptr,"Scale factors:"))
				{
					state=0; //skip the rest
					fseek(f,offset,SEEK_SET);
					continue;
				}
				state++;
			}
			break;
			case 6: //Scale factors body
			{
				if(!isdigit(*ptr) && *ptr!='-')
				{
					state=0;
					fseek(f,offset,SEEK_SET);
					list.push_back(elem);
					continue;
				}
				strs.clear();
				tokenize(ptr," \t",strs);
				for(int i=0;i<strs.size();i++)
				{
					double v=strtod(strs[i].c_str(),NULL);
					if(fabs(v)<precision)
						v=fabs(v);
					elem.scale.push_back(v);
				}
			}
			break;
		}
	}
	if(state)
		list.push_back(elem);
	std::sort(list.begin(), list.end(), elem_sort);
}


bool process_elems(const std::string& hdr,ELEMLIST& src,const std::string& output)
{
	ELEMLIST::iterator isrc;
	FILE *fout=stdout;
	int nodes=0;
	int skip;

	size_t pos=hdr.find("#Nodes=");
	if(pos!=std::string::npos)
		nodes=strtol(hdr.c_str()+pos+9,NULL,10);

	if(output.size())
		fout=fopen(output.c_str(),"wt");
	if(!fout)
	{
		fprintf(stderr,"Error opening output file %s\n",output.c_str());
		return false;
	}
	if(add_header)
		fprintf(fout,"%s\n",hdr.c_str());
	for(isrc=src.begin();isrc!=src.end();++isrc)
	{
		fprintf(fout," Element:        ");
		for(int i=0;i<isrc->id.size();i++)
			fprintf(fout," %d",isrc->id[i]);

		fprintf(fout," Values:\n");
		skip=(nodes?nodes:1);
		for(int i=0;i<isrc->values.size();i+=skip)
		{
			fprintf(fout,"  ");
			for(int j=0;j<skip;j++)
				fprintf(fout," %lf",isrc->values[i+j]);
			fprintf(fout,"\n");
		}
		fprintf(fout,"\n");
		fprintf(fout," Nodes:\n    ");
		for(int i=0;i<isrc->nodes.size();i++)
			fprintf(fout," %d",isrc->nodes[i]);
		fprintf(fout,"\n");
		fprintf(fout," Scale factors:\n   ");
		for(int i=0;i<isrc->scale.size();i+=skip)
		{
			fprintf(fout," %lf",isrc->scale[i]);
		}
		fprintf(fout,"\n");
	}
	fclose(fout);
	return true;
}


bool load_file(const char *filename,std::string& hdr,ELEMLIST& elems)
{
	FILE *f=fopen(filename,"rb");
	if(!f)
		return false;

	read_header(f,stoppers[ELEMENTS_TYPE],hdr);
	read_body_elems(f,elems);
	fclose(f);
	return true;
}

bool load_file(const char *filename,std::string& hdr,NODELIST& nodes)
{
	FILE *f=fopen(filename,"rb");
	if(!f)
		return false;

	read_header(f,stoppers[NODES_TYPE],hdr);
	read_body_node(f,nodes);
	fclose(f);
	return true;
}



void usage(const char *name)
{
	fprintf(stderr,"Usage: %s -e|-n <list_of_element_or_nodes_files> <-c list_of_files_to_compare to> [-r (to add header to the output)] [-q (for quiet operations)]\n",name);
}

int main(int argc, char *argv[])
{
	STRLIST src;
	STRLIST *pCurrentList=NULL;
	FILETYPE ftype=UNDEF_TYPE;
	ELEMLIST elems_src;
	NODELIST nodes_src;
	bool res=false;
	std::string hdr;
	std::string output;

	for(int i=1;i<argc;i++)
	{
		if(argv[i][0]=='-')
		{
			switch(argv[i][1])
			{
				case 'e':
						if(ftype!=UNDEF_TYPE)
						{
							usage(argv[0]);
							exit(1);
						}
						pCurrentList=&src;
						ftype=ELEMENTS_TYPE;
						break;
				case 'n':
						if(ftype!=UNDEF_TYPE)
						{
							usage(argv[0]);
							exit(1);
						}
						pCurrentList=&src;
						ftype=NODES_TYPE;
						break;
				case 'o':
						output=argv[++i];
						break;
				case 'r':
						add_header=true;
						break;
				default:
						pCurrentList=NULL;
			}
		}
		else if(pCurrentList)
		{
			//Put name on the current list
			pCurrentList->push_back(string(argv[i]));
		}
	}

	if(ftype==UNDEF_TYPE)
	{
		usage(argv[0]);
		exit(1);
	}

	expand_file_list(src);
	for(int i=0;i<src.size();i++)
	{
		res=(ftype==ELEMENTS_TYPE?load_file(src[i].c_str(),hdr,elems_src):load_file(src[i].c_str(),hdr,nodes_src));
		if(!res)
		{
			fprintf(stderr,"Error opening file %s\n",src[i].c_str());
			exit(1);
		}
	}
	if(ftype==NODES_TYPE)
			res=process_nodes(hdr,nodes_src,output);
	else
			res=process_elems(hdr,elems_src,output);
	
	return (res?0:-1);
}

