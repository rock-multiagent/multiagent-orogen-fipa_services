require 'readline'
require 'orocos'
include Orocos

Orocos.initialize

deployment_name = 'test_mts'
prefix = ARGV[0]

Orocos.run deployment_name , :wait => 10, :cmdline_args => { :prefix => prefix } do |p1|

begin
    mts_module = p1.task "#{prefix}mts"
rescue Orocos::NotFound
    print 'Deployment not found.'
    raise
end

    mts_module.avahi_type = "_test_mts._tcp"
    mts_module.avahi_port = 12000
    mts_module.avahi_ttl = 0
    mts_module.configure
    mts_module.start

    Readline::readline("Press enter to exit")
end 
