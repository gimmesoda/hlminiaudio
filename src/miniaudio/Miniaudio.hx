package miniaudio;

import haxe.io.Bytes;

@:hlNative('miniaudio', 'buffer_')
abstract Buffer(BufferImpl) from BufferImpl to BufferImpl
{
	public function dispose() {}

	public static inline function fromBytes(bytes:Bytes)
	{
		#if hlopus
		if (isOpusStream(bytes))
			return fromOpusBytes(bytes);
		#end
		return _fromBytes(bytes, bytes.length);
	}

	public static inline function fromPCMFloat(bytes:Bytes, channels:Int, sampleRate:Int)
	{
		return _fromPCMFloat(bytes, bytes.length, channels, sampleRate);
	}

	@:hlNative('miniaudio', 'buffer_from_bytes')
	private static function _fromBytes(bytes:hl.Bytes, size:Int):Buffer
	{
		return null;
	}

	@:hlNative('miniaudio', 'buffer_from_pcm_float')
	private static function _fromPCMFloat(bytes:hl.Bytes, size:Int, channels:Int, sampleRate:Int):Buffer
	{
		return null;
	}

	#if hlopus
	private static function fromOpusBytes(bytes:Bytes):Buffer
	{
		var decoder = new hlopus.Decoder(bytes);
		var pcm = decoder.decodeAll(hlopus.SampleFormat.F32);
		return fromPCMFloat(pcm, decoder.channels, decoder.samplingRate);
	}

	private static function isOpusStream(bytes:Bytes):Bool
	{
		if (bytes.length < 36)
			return false;

		if (bytes.get(0) != 'O'.code || bytes.get(1) != 'g'.code || bytes.get(2) != 'g'.code || bytes.get(3) != 'S'.code)
			return false;

		var numSegments = bytes.get(26);
		var dataOffset = 27 + numSegments;
		if (bytes.length < dataOffset + 8)
			return false;

		return bytes.get(dataOffset) == 'O'.code
			&& bytes.get(dataOffset + 1) == 'p'.code
			&& bytes.get(dataOffset + 2) == 'u'.code
			&& bytes.get(dataOffset + 3) == 's'.code
			&& bytes.get(dataOffset + 4) == 'H'.code
			&& bytes.get(dataOffset + 5) == 'e'.code
			&& bytes.get(dataOffset + 6) == 'a'.code
			&& bytes.get(dataOffset + 7) == 'd'.code;
	}
	#end
}

private typedef BufferImpl = hl.Abstract<'ma_audio_buffer*'>;

@:hlNative('miniaudio', 'sound_group_')
abstract SoundGroup(SoundGroupImpl) from SoundGroupImpl to SoundGroupImpl
{
	public var volume(get, set):Float;
	public var pan(get, set):Float;
	// TODO: pan mode (??)
	public var pitch(get, set):Float;

	// TODO: spatialization (??)

	public inline function new(?parent:SoundGroup)
	{
		this = _init(parent);
	}

	public function start():Bool
	{
		return false;
	}

	public function stop():Bool
	{
		return false;
	}

	@:hlNative('miniaudio', 'sound_group_init')
	private static function _init(?parent:SoundGroup):SoundGroup
	{
		return null;
	}

	private function get_volume():Float
	{
		return 0;
	}

	private function set_volume(v:Float):Float
	{
		return 0;
	}

	private function get_pan():Float
	{
		return 0;
	}

	private function set_pan(v:Float):Float
	{
		return 0;
	}

	private function get_pitch():Float
	{
		return 0;
	}

	private function set_pitch(v:Float):Float
	{
		return 0;
	}
}

private typedef SoundGroupImpl = hl.Abstract<'ma_sound_group*'>;

@:hlNative('miniaudio', 'sound_')
abstract Sound(SoundImpl) from SoundImpl to SoundImpl
{
	public var volume(get, set):Float;
	public var pan(get, set):Float;
	// TODO: pan mode (??)
	public var pitch(get, set):Float;
	// TODO: spatialization (??)
	public var time(get, never):Float;

	public inline function new(buffer:Buffer, ?parent:SoundGroup)
	{
		this = _init(buffer, parent);
	}

	public function dispose() {}

	public function start():Bool
	{
		return false;
	}

	public function stop():Bool
	{
		return false;
	}

	@:hlNative('miniaudio', 'sound_init')
	private static function _init(buffer:Buffer, ?parent:SoundGroup):Sound
	{
		return null;
	}

	private function get_volume():Float
	{
		return 0;
	}

	private function set_volume(v:Float):Float
	{
		return 0;
	}

	private function get_pan():Float
	{
		return 0;
	}

	private function set_pan(v:Float):Float
	{
		return 0;
	}

	private function get_pitch():Float
	{
		return 0;
	}

	private function set_pitch(v:Float):Float
	{
		return 0;
	}

	private function get_time():Float
	{
		return 0;
	}
}

private typedef SoundImpl = hl.Abstract<'ma_sound*'>;

@:hlNative('miniaudio')
class Miniaudio
{
	public static function init():Bool
	{
		return false;
	}

	public static function uninit() {}

	public static inline function describeLastError():String
	{
		return @:privateAccess String.fromUTF8(_describeLastError());
	}

	@:hlNative('miniaudio', 'describe_last_error')
	private static function _describeLastError():hl.Bytes
	{
		return null;
	}
}
