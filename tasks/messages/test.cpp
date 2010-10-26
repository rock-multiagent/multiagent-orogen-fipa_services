#include <iostream>
#include <string>

#include <fipa_message.h>

int main(int argc, char* argv[])
{
    using namespace root;

    // Create fipa message with predefined parameters.
    FipaMessage fipa;

    // mod1 should send to mod2 and mod3 a "Hi guys!".
    try{
        if(argc > 1)
            fipa.setMessage(argv[1]);
        else
            fipa.setMessage("PERFORMATIVE inform SENDER mod1 RECEIVER START mod2 mod3 STOP CONTENT START Hi guys! STOP");


        // Encode message and return byte string.
        std::string msg_encoded = fipa.encode();    
        std::cout << msg_encoded << std::endl;

        // Decode message.
        fipa.decode(msg_encoded);

        // Read answer.
        std::vector<std::string>& entries = fipa.getEntry("RECEIVER");
        for(int i=0; i<entries.size(); i++)
            std::cout << entries[i] << std::endl;

        // Change receiver.
        //entries.clear(); // or
        fipa.clear("RECEIVER SENDER"); // Clears receiver and sender parameters.
        entries.push_back("new_receiver");

        // Encode and decode again with new receiver.
        std::cout << (msg_encoded = fipa.encode()) << std::endl;
        fipa.decode(msg_encoded);
        entries = fipa.getEntry("RECEIVER");
        std::cout << "New receiver: " << entries[0] << std::endl;

        // Use aclMessage directly.
        fipa::acl::ACLMessage* aclMSG = fipa.getACLMessage();
        std::cout << "ACLMessage receiver: " <<  aclMSG->getAllReceivers()[0].getName() << std::endl;

        // Clear parameter-map.
        fipa.clear();
        entries = fipa.getEntry("RECEIVER");
        std::cout << "Entry 'receiver' size: " << entries.size() << std::endl;

    } catch (MessageException& e) {
        std::cout << e.what() << std::endl;
    }
}
