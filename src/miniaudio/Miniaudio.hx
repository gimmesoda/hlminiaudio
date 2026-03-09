package miniaudio;

#if hlopenal
#error "hlminiaudio and hlopenal is conflicting; remove one of them from the build."
#end
#if hlopus
#error "`hlminiaudio` provides native Opus support; remove `-lib hlopus` from the build."
#end
import haxe.io.Bytes;

typedef DecodedAudio =
{
	bytes:Bytes,
	channels:Int,
	sampleRate:Int,
	samples:Int,
	floatFormat:Bool,
}

enum abstract PanMode(Int) from Int to Int
{
	final Balance = 0;
	final Pan = 1;
}

@:hlNative('miniaudio', 'buffer_')
abstract Buffer(BufferImpl) from BufferImpl to BufferImpl
{
	public function dispose() {}

	public static inline function fromBytes(bytes:Bytes)
	{
		return _fromBytes(bytes, bytes.length);
	}

	public static inline function fromPCMFloat(bytes:Bytes, channels:Int, sampleRate:Int)
	{
		return _fromPCMFloat(bytes, bytes.length, channels, sampleRate);
	}

	public static inline function fromPCM16(bytes:Bytes, channels:Int, sampleRate:Int)
	{
		return _fromPCM16(bytes, bytes.length, channels, sampleRate);
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

	@:hlNative('miniaudio', 'buffer_from_pcm_s16')
	private static function _fromPCM16(bytes:hl.Bytes, size:Int, channels:Int, sampleRate:Int):Buffer
	{
		return null;
	}
}

private typedef BufferImpl = hl.Abstract<'ma_audio_buffer*'>;

@:hlNative('miniaudio', 'sound_group_')
abstract SoundGroup(SoundGroupImpl) from SoundGroupImpl to SoundGroupImpl
{
	public var volume(get, set):Float;
	public var pan(get, set):Float;
	public var panMode(get, set):PanMode;
	public var pitch(get, set):Float;
	public var spatializationEnabled(get, set):Bool;

	public inline function new(?parent:SoundGroup)
	{
		this = _init(parent);
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

	private function get_panMode():PanMode
	{
		return Balance;
	}

	private function set_panMode(v:PanMode):PanMode
	{
		return v;
	}

	private function get_pitch():Float
	{
		return 0;
	}

	private function set_pitch(v:Float):Float
	{
		return 0;
	}

	private function get_spatializationEnabled():Bool
	{
		return false;
	}

	private function set_spatializationEnabled(v:Bool):Bool
	{
		return v;
	}
}

private typedef SoundGroupImpl = hl.Abstract<'ma_sound_group*'>;

@:hlNative('miniaudio', 'sound_')
abstract Sound(SoundImpl) from SoundImpl to SoundImpl
{
	public var volume(get, set):Float;
	public var pan(get, set):Float;
	public var panMode(get, set):PanMode;
	public var pitch(get, set):Float;
	public var spatializationEnabled(get, set):Bool;
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

	@:hlNative('miniaudio', 'sound_seek_samples')
	public function seekSamples(v:Int):Int
	{
		return v;
	}

	@:hlNative('miniaudio', 'sound_get_cursor_samples')
	public function getCursorSamples():Int
	{
		return 0;
	}

	@:hlNative('miniaudio', 'sound_is_playing')
	public function isPlaying():Bool
	{
		return false;
	}

	@:hlNative('miniaudio', 'sound_init')
	@:noCompletion
	private static function _init(buffer:Buffer, ?parent:SoundGroup):Sound
	{
		return null;
	}

	@:noCompletion
	private function get_volume():Float
	{
		return 0;
	}

	@:noCompletion
	private function set_volume(v:Float):Float
	{
		return 0;
	}

	@:noCompletion
	private function get_pan():Float
	{
		return 0;
	}

	@:noCompletion
	private function set_pan(v:Float):Float
	{
		return 0;
	}

	@:noCompletion
	private function get_panMode():PanMode
	{
		return Balance;
	}

	@:noCompletion
	private function set_panMode(v:PanMode):PanMode
	{
		return v;
	}

	@:noCompletion
	private function get_pitch():Float
	{
		return 0;
	}

	@:noCompletion
	private function set_pitch(v:Float):Float
	{
		return 0;
	}

	@:noCompletion
	private function get_spatializationEnabled():Bool
	{
		return false;
	}

	@:noCompletion
	private function set_spatializationEnabled(v:Bool):Bool
	{
		return v;
	}

	@:noCompletion
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

	public static inline function decodeToPCMFloat(bytes:Bytes):DecodedAudio
	{
		final decoded = _decodeToPCMFloat(bytes, bytes.length);
		final channels = _decodedChannels();
		final sampleRate = _decodedSampleRate();
		final samples = _decodedSamples();
		final byteCount = samples * channels * 4;
		if (decoded == null)
			return null;

		return {
			bytes: @:privateAccess new Bytes(decoded, byteCount),
			channels: channels,
			sampleRate: sampleRate,
			samples: samples,
			floatFormat: true,
		};
	}

	public static inline function decodeToPCM16(bytes:Bytes):DecodedAudio
	{
		final decoded = _decodeToPCM16(bytes, bytes.length);
		final channels = _decodedChannels();
		final sampleRate = _decodedSampleRate();
		final samples = _decodedSamples();
		final byteCount = samples * channels * 2;
		if (decoded == null)
			return null;

		return {
			bytes: @:privateAccess new Bytes(decoded, byteCount),
			channels: channels,
			sampleRate: sampleRate,
			samples: samples,
			floatFormat: false,
		};
	}

	public static inline function describeLastError():String
	{
		return @:privateAccess String.fromUTF8(_describeLastError());
	}

	@:hlNative('miniaudio', 'describe_last_error')
	@:noCompletion
	private static function _describeLastError():hl.Bytes
	{
		return null;
	}

	@:hlNative('miniaudio', 'decode_pcm_float')
	@:noCompletion
	private static function _decodeToPCMFloat(bytes:hl.Bytes, size:Int):hl.Bytes
	{
		return null;
	}

	@:hlNative('miniaudio', 'decode_pcm_s16')
	@:noCompletion
	private static function _decodeToPCM16(bytes:hl.Bytes, size:Int):hl.Bytes
	{
		return null;
	}

	@:hlNative('miniaudio', 'decoded_channels')
	@:noCompletion
	private static function _decodedChannels():Int
	{
		return 0;
	}

	@:hlNative('miniaudio', 'decoded_sample_rate')
	@:noCompletion
	private static function _decodedSampleRate():Int
	{
		return 0;
	}

	@:hlNative('miniaudio', 'decoded_samples')
	@:noCompletion
	private static function _decodedSamples():Int
	{
		return 0;
	}
}
