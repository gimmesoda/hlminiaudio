package hxd.snd.miniaudio;

import haxe.ds.ObjectMap;
import haxe.io.Bytes;
import hxd.snd.Data.SampleFormat;
import hxd.snd.Driver.DriverFeature;
import hxd.snd.openal.AudioTypes.BufferHandle;
import hxd.snd.openal.AudioTypes.SourceHandle;
import miniaudio.Miniaudio;
import miniaudio.Miniaudio.Buffer as MiniBuffer;
import miniaudio.Miniaudio.Sound as MiniSound;

class Driver implements hxd.snd.Driver
{
	final sourceSounds = new ObjectMap<SourceHandle, MiniSound>();
	final sourceVolumes = new ObjectMap<SourceHandle, Float>();
	final sourceQueuedStarts = new ObjectMap<SourceHandle, Int>();
	final sourceProcessed = new ObjectMap<SourceHandle, Bool>();
	final buffers = new ObjectMap<BufferHandle, MiniBuffer>();
	final bufferSamples = new ObjectMap<BufferHandle, Int>();
	final bufferRates = new ObjectMap<BufferHandle, Int>();

	public function new()
	{
		if (!Miniaudio.init())
			throw Miniaudio.describeLastError();
	}

	public function hasFeature(f:DriverFeature):Bool
	{
		return switch (f)
		{
			case MasterVolume: false;
		};
	}

	public function setMasterVolume(value:Float):Void
	{
		value;
	}

	public function setListenerParams(position:h3d.Vector, direction:h3d.Vector, up:h3d.Vector, ?velocity:h3d.Vector):Void
	{
		position;
		direction;
		up;
		velocity;
	}

	public function createSource():SourceHandle
	{
		final source = new SourceHandle();
		sourceVolumes.set(source, 1.0);
		sourceQueuedStarts.set(source, 0);
		sourceProcessed.set(source, false);
		return source;
	}

	public function playSource(source:SourceHandle):Void
	{
		final sound = sourceSounds.get(source);
		if (sound != null)
		{
			sound.start();
			source.playing = true;
		}
	}

	public function stopSource(source:SourceHandle):Void
	{
		final sound = sourceSounds.get(source);
		if (sound != null)
			sound.stop();
		source.playing = false;
	}

	public function setSourceVolume(source:SourceHandle, value:Float):Void
	{
		sourceVolumes.set(source, value);
		final sound = sourceSounds.get(source);
		if (sound != null)
			sound.volume = value;
	}

	public function destroySource(source:SourceHandle):Void
	{
		final sound = sourceSounds.get(source);
		if (sound != null)
		{
			sound.stop();
			sound.dispose();
			sourceSounds.remove(source);
		}
		sourceVolumes.remove(source);
		sourceQueuedStarts.remove(source);
		sourceProcessed.remove(source);
		source.playing = false;
	}

	public function createBuffer():BufferHandle
	{
		return new BufferHandle();
	}

	public function setBufferData(buffer:BufferHandle, data:Bytes, size:Int, format:SampleFormat, channelCount:Int, samplingRate:Int):Void
	{
		final sourceBytes = if (size == data.length) data else data.sub(0, size);
		final oldBuffer = buffers.get(buffer);
		if (oldBuffer != null)
			oldBuffer.dispose();

		final newBuffer = switch (format)
		{
			case F32:
				MiniBuffer.fromPCMFloat(sourceBytes, channelCount, samplingRate);
			case I16:
				MiniBuffer.fromPCM16(sourceBytes, channelCount, samplingRate);
			case UI8:
				MiniBuffer.fromPCM16(convertUI8ToI16(sourceBytes), channelCount, samplingRate);
		};
		buffers.set(buffer, newBuffer);

		bufferSamples.set(buffer, Std.int(size / bytesPerSample(format, channelCount)));
		bufferRates.set(buffer, samplingRate);
	}

	public function destroyBuffer(buffer:BufferHandle):Void
	{
		final audioBuffer = buffers.get(buffer);
		if (audioBuffer != null)
		{
			audioBuffer.dispose();
			buffers.remove(buffer);
		}
		bufferSamples.remove(buffer);
		bufferRates.remove(buffer);
	}

	public function queueBuffer(source:SourceHandle, buffer:BufferHandle, sampleStart:Int, endOfStream:Bool):Void
	{
		final oldSound = sourceSounds.get(source);
		if (oldSound != null)
		{
			oldSound.stop();
			oldSound.dispose();
		}

		final audioBuffer = buffers.get(buffer);
		if (audioBuffer == null)
			return;
		final sound = new MiniSound(audioBuffer, null);
		sound.volume = sourceVolumes.exists(source) ? sourceVolumes.get(source) : 1.0;
		sound.seekSamples(sampleStart);
		sourceSounds.set(source, sound);
		sourceQueuedStarts.set(source, sampleStart);
		source.sampleOffset = 0;
		sourceProcessed.set(source, false);
		buffer.isEnd = endOfStream;
	}

	public function unqueueBuffer(source:SourceHandle, buffer:BufferHandle):Void
	{
		if (buffer.isEnd)
			source.sampleOffset = 0;
		else
			source.sampleOffset += bufferSamples.exists(buffer) ? bufferSamples.get(buffer) : 0;

		final sound = sourceSounds.get(source);
		if (sound != null)
		{
			sound.stop();
			sound.dispose();
			sourceSounds.remove(source);
		}

		source.playing = false;
		sourceProcessed.set(source, false);
	}

	public function getProcessedBuffers(source:SourceHandle):Int
	{
		final sound = sourceSounds.get(source);
		if (sound == null || sourceProcessed.get(source) || !source.playing)
			return 0;

		if (!sound.isPlaying())
		{
			sourceProcessed.set(source, true);
			return 1;
		}

		return 0;
	}

	public function getPlayedSampleCount(source:SourceHandle):Int
	{
		final sound = sourceSounds.get(source);
		if (sound == null)
			return source.sampleOffset;

		final played = sound.getCursorSamples() - sourceQueuedStarts.get(source);
		return source.sampleOffset + (played < 0 ? 0 : played);
	}

	public function update():Void {}

	public function dispose():Void
	{
		Miniaudio.uninit();
	}

	public function getEffectDriver(type:String):hxd.snd.Driver.EffectDriver<Dynamic>
	{
		type;
		return new hxd.snd.Driver.EffectDriver<Dynamic>();
	}

	static function bytesPerSample(format:SampleFormat, channels:Int):Int
	{
		return switch (format)
		{
			case UI8: channels;
			case I16: channels * 2;
			case F32: channels * 4;
		};
	}

	static function convertUI8ToI16(data:Bytes):Bytes
	{
		final out = Bytes.alloc(data.length * 2);
		for (i in 0...data.length)
		{
			var sample = ((data.get(i) - 128) << 8);
			out.setUInt16(i * 2, sample & 0xFFFF);
		}
		return out;
	}
}
