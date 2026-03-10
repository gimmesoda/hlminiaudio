package miniaudio.heaps;

import haxe.io.Path;
import hxd.fs.FileEntry;
#if hlopus
import hxd.fmt.opus.Data as OpusData;
#end

class AudioBuffer extends hxd.res.Sound
{
	var cachedData:Null<hxd.snd.Data>;

	public function new(entry:FileEntry)
	{
		super(entry);
		entry.watch(() -> cachedData = null);
	}

	override public function getData():hxd.snd.Data
	{
		if (cachedData != null)
			return cachedData;

		final bytes = entry.getBytes();
		final ext = Path.extension(entry.path).toLowerCase();

		cachedData = switch (ext)
		{
			case 'opus':
				loadOpus(bytes);
			case 'ogg', 'flac', 'wav', 'mp3':
				loadWithMiniaudio(bytes);
			default:
				super.getData();
		};

		if (cachedData == null)
			throw '${entry.path}: ${miniaudio.Miniaudio.describeLastError()}';

		return cachedData;
	}

	override public function dispose()
	{
		stop();
		cachedData = null;
	}

	static function loadWithMiniaudio(bytes:haxe.io.Bytes):hxd.snd.Data
	{
		final decoded = miniaudio.Miniaudio.decodeToPCMFloat(bytes);
		if (decoded == null)
			return null;

		return new AudioData(decoded.bytes, decoded.channels, decoded.sampleRate, decoded.samples, decoded.floatFormat);
	}

	static function loadOpus(bytes:haxe.io.Bytes):hxd.snd.Data
	{
		#if hlopus
		return new OpusData(bytes);
		#else
		throw 'Opus is not supported: install `hlopus` and rebuild.';
		#end
	}
}
