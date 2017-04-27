require 'fipa-message'

# v 1.0
#
# - added time_format to add nanoseconds to timestamp
# - fix job id counting
# - add generate code for random message data to avoid 'warm-up' effects

module FIPA
    MAX_EXPONENT = 20
    EPOCHS = 10

    class Benchmark
        attr_reader :job_id
        attr_reader :mts
        attr_reader :letter_writer
        attr_reader :letter_reader

        attr_reader :from
        attr_reader :to
        attr_reader :contents
        attr_reader :known_agents

        attr_reader :time_format

        CHARSET = [*"a".."z",*"A".."Z",*"0".."9"]

        def initialize(mts_task, from, to)
            @job_id = 0
            @mts = mts_task
            @letter_writer = @mts.letters.writer :type => :buffer, :size => 10
            @letter_reader = @mts.port(from).reader :type => :buffer, :size => 10

            @from = from
            @to = to
            @contents = Hash.new
            @known_agents = Hash.new

            @time_format = "%Y%m%d-%H%M%S.%N"
        end

        def get_timestamp
            Time.now.strftime(@time_format)
        end

        # Geneate random block of data based of a given character set
        # from: http://www.blackbytes.info/2015/03/ruby-random/
        def generate_code(number)
           Array.new(number) { CHARSET.sample }.join
        end

        def identify_receiver_agents(timeout_in_s = 30)
            letter = create_sender_letter(from, ".*")
            letter_writer.write(letter)

            wait_for_appearance(timeout_in_s)
        end

        def wait_for_appearance(timeout_in_s)
            puts "Start waiting for wait_for_appearance of agents with timeout: #{timeout_in_s}"
            start = Time.now
            stop = false
            while !stop
                if (Time.now - start) > timeout_in_s
                    stop = true
                end

                if letter = letter_reader.read_new
                    identified_agent = letter.getACLMessage.getSender.getName
                    puts "#{from}: received letter with content from: #{identified_agent}"
                    if identified_agent !~ /^mts/
                        @known_agents[identified_agent] = true
                    else
                        puts "    that is a message from an mts"
                    end
                end
            end
            puts "Done waiting for agents (timeout after #{timeout_in_s} seconds -- identified: #{@known_agents.keys.join(",")}"
        end

        def wait_for_all_agents(timeout_in_s = 60)
            puts "Start waiting for all agents with timeout: #{timeout_in_s}"
            start = Time.now
            stop = false
            wait_list = known_agents.keys
            while !stop
                if (Time.now - start) > timeout_in_s
                    stop = true
                    if not wait_list.empty?
                        raise RuntimeError, "The following agents did not respond within the timeout of #{timeout_in_s}: #{known_agents}"
                    end
                end

                if letter = letter_reader.read_new
                    identified_agent = letter.getFrom.getName
                    puts "#{from}: received letter from: #{identified_agent}"
                    wait_list.delete identified_agent
                    if wait_list.empty?
                        stop = true
                    end
                end
            end
            puts "Done waiting for agents (timeout after #{timeout_in_s} seconds -- all responses received"
        end

        def prepare_contents
            (0..MAX_EXPONENT).each do |i|
                content_size = 2**i
                puts "Content size: #{(content_size)/1024.0} kB"
                # Generate random content of particular size
                @contents[content_size] = generate_code(content_size)
            end
        end

        def create_sender_message(from, to, content_size = 1, protocol = "ECHO", language = "BENCHMARK")
            msg = FIPA::ACLMessage.new
            msg.setPerformative(:request)
            msg.setProtocol(protocol)
            msg.setLanguage(language)
            #msg.setContent(@contents[content_size])
            msg.setContent( generate_code(content_size) )
            msg.addReceiver(FIPA::AgentId.new(to))
            msg.setSender(FIPA::AgentId.new(from))
            msg.addReplyTo(FIPA::AgentId.new(from))
            msg.setConversationID("conversation-id_#{get_timestamp}-#{@job_id}")
            @job_id += 1
            msg
        end

        def create_sender_letter(from, to, content_size = 1, protocol = "ECHO", language = "BENCHMARK", representation = FIPARepresentation::BITEFFICIENT)
            msg = create_sender_message(from, to, content_size, protocol, language)
            env = FIPA::ACLEnvelope.new
            env.insert(msg, representation)
            env
        end

        def send_letter(from, to, content_size = 1)
            puts "Sending letter from #{from} to #{to} with size #{content_size}"
            letter = create_sender_letter(from, to, content_size) 
            letter_writer.write(letter)
            puts "Written letter to #{to}"
        end

        def send
            identify_receiver_agents

            (1..EPOCHS).each do |epoch|
                (0..MAX_EXPONENT).each do |i|
                    puts "Send letter - epoch: #{epoch} content size #{2**i}"
                    send_letter(from, to, 2**i)

                    puts "Send wait for agents"
                    wait_for_all_agents
                end
            end
        end
    end
end
