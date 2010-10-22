#include <iostream>
#include <string>

#include <fipa_message.h>

int main(int argc, char* argv[])
{
    using namespace root;
    std::cout << "Testing FipaMessage..." << std::endl;
    FipaMessage fipa;
    
    std::cout << "Set message" << std::endl;
    bool ret = fipa.setMessage("SENDER sender RECEIVER receiver1 \
            RECEIVER receiver2 PERFORMATIVE inform");

    std::cout << "Encode message" << std::endl;
    std::string str = fipa.encodeMessage();
    std::cout << str << std::endl;

    std::cout << "Decode message" << std::endl;
    if(!fipa.decodeMessage(str))
        std::cerr << "Error decoding message." << std::endl;
    else 
         std::cout << "Message decoded" << std::endl;
    
    std::cout << "Request decoded entries" << std::endl;
    std::vector<std::string>* entries = NULL;
    if(fipa.getEntriesP("RECEIVER", &entries) > 0)
    {
        if(entries == NULL)
        {
            std::cout << "entrie NULL!" << std::endl;
            exit(1);
        } else {
            std::cout << "entries size: " << entries->size() << std::endl;
        }

        entries->push_back("receiver3");
        std::cout << "Receivers: ";
        for(int i=0; i<entries->size(); i++)
        {
            std::cout << entries->at(i) << " ";
        }
        std::cout << std::endl;
    }
}
