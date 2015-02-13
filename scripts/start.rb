require 'readline'
require 'orocos'
include Orocos

Orocos.initialize

Orocos.run "fipa_services::MessageTransportTask" => "mts_0" do

    begin
        mts_module = TaskContext.get "mts_0"
    rescue Orocos::NotFound
        print 'Deployment not found.'
        raise
    end

    mts_module.configure
    mts_module.start

    mts_module.addReceiver("local_client", true)

    Readline::readline("Press ENTER to proceed")

end 
