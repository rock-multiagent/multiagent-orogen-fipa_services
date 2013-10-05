# This is a typelib plugin which relies on the FipaMessage library 
# It allows to send fipa letters from ruby to orocos ports of type
# /fipa/SerializedLetter
begin 
    require 'fipa-message'
    Typelib.convert_to_ruby '/fipa/SerializedLetter', FIPA::ACLEnvelope do |typelib_value|
        case typelib_value.representation
        when :BITEFFICIENT
            return FIPA::Utils.deserialize_envelope(typelib_value.data.to_a)
        when :UNKNOWN
            return FIPA::ACLEnvelope.new
        else
            raise ArgumentError, "Typelib: extension 'mts' does not support other representations than BITEFFICIENT"
        end
    end

    Typelib.convert_from_ruby FIPA::ACLEnvelope, '/fipa/SerializedLetter' do |value, typelib_type|
        # create an object of type '/fipa/SerializedLetter
        letter = typelib_type.new
        letter.data = FIPA::Utils.serialize_envelope(value)
        letter.representation = :BITEFFICIENT
        letter.timestamp = Time.now
        letter
    end

rescue LoadError
    STDERR.puts "The fipa-message library is not present, I am thus unable to support ruby conversion for fipa messages"
end



