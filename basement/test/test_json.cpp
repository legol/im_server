#include <stdio.h>
#include <fstream>
#include "json/json.h"

int loadPlugIn(const std::string &strPlugIn)
{
	std::cout << "plugin=" << strPlugIn << std::endl;
	return 0;
};

int setIndentLength(int iIndent)
{
	std::cout << "indent=" << iIndent << std::endl;
	return 0;
}

int setIndentUseSpace(bool bUseSpace)
{
	std::cout << "use_space=" << bUseSpace << std::endl;
	return 0;
}

int main()
{
	std::ifstream document("./test_json.json");

	Json::Value root;   // will contains the root value after parsing.
	Json::Reader reader;
	bool parsingSuccessful = reader.parse( document, root );
	if ( !parsingSuccessful )
	{
		// report to the user the failure and their locations in the document.
		std::cout  << "Failed to parse configuration\n"
			<< reader.getFormatedErrorMessages();
		return -1;
	}

	// Get the value of the member of root named 'encoding', return 'UTF-8' if there is no
	// such member.
	std::string encoding = root.get("encoding", "UTF-8" ).asString();
	// Get the value of the member of root named 'encoding', return a 'null' value if
	// there is no such member.
	const Json::Value plugins = root["plug-ins"];
	for ( int index = 0; index < (int)(plugins.size()); ++index )  // Iterates over the sequence elements.
		loadPlugIn( plugins[index].asString() );

	setIndentLength( root["indent"].get("length", 3).asInt() );
	setIndentUseSpace( root["indent"].get("use_space", true).asBool() );

	// ...
	// At application shutdown to make the new configuration document:
	// Since Json::Value has implicit constructor for all value types, it is not
	// necessary to explicitly construct the Json::Value object:
	root["encoding"] = std::string("UTF-8");
	root["indent"]["length"] = 99;
	root["indent"]["use_space"] = false;

	Json::StyledWriter writer;
	// Make a new JSON document for the configuration. Preserve original comments.
	std::string outputConfig = writer.write( root );

	// You can also use streams.  This will put the contents of any JSON
	// stream at a particular sub-value, if you'd like.
	//std::cin >> root["subtree"];

	// And you can write to a stream, using the StyledWriter automatically.
	std::cout << root;

	return 0;
}
