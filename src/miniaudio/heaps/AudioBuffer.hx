package miniaudio.heaps;

import haxe.io.Bytes;
import haxe.io.Path;
import hxd.fs.FileEntry;
import hxd.res.Resource;
import hxd.snd.Data.SampleFormat;
#if miniaudio_has_hlopus
import hxd.fmt.opus.Data as OpusData;
#end

class AudioBuffer extends Resource
{
	private var loaded:Bool = false;

	private var buffer:Miniaudio.Buffer;

	public function new(entry:FileEntry)
	{
		super(entry);

		entry.watch(() ->
		{
			if (buffer != null)
				buffer.dispose();
			buffer = null;
			loaded = false;
		});
	}

	public function load():Miniaudio.Buffer
	{
		if (!loaded)
		{
			final ext = Path.extension(entry.path).toLowerCase();
			buffer = switch (ext)
			{
				case 'opus':
					loadOpus(entry.getBytes());
				default:
					Miniaudio.Buffer.fromBytes(entry.getBytes());
			};

			if (buffer == null)
			{
				if (ext == 'opus')
					return null;

				throw '${entry.path}: ${Miniaudio.describeLastError()}';
			}

			loaded = true;
		}

		return buffer;
	}

	private static function loadOpus(bytes:Bytes):Miniaudio.Buffer
	{
		#if miniaudio_has_hlopus
		final opus = new OpusData(bytes);
		final pcm = opus.resample(opus.samplingRate, SampleFormat.F32, opus.channels);
		final raw = Bytes.alloc(pcm.samples * pcm.channels * 4);
		pcm.decode(raw, 0, 0, pcm.samples);
		return Miniaudio.Buffer.fromPCMFloat(raw, pcm.channels, pcm.samplingRate);
		#else
		Sys.println('Opus is not supported: install `hlopus` and rebuild.');
		return null;
		#end
	}
}
