require 'readline'
require 'orocos'
include Orocos

Orocos.initialize

deployment_name = 'test_root'
prefix = ARGV[0]

Orocos.run deployment_name , :wait => 10, :cmdline_args => { :prefix => prefix } do |p1|

begin
    root_module = p1.task "#{prefix}ROOT"
rescue Orocos::NotFound
    print 'Deployment not found.'
    raise
end

    root_module.avahi_type = "_test_root._tcp"
    root_module.avahi_port = 12000
    root_module.avahi_ttl = 0
    root_module.configure
    root_module.start

    Readline::readline("Press enter to exit")
end 
