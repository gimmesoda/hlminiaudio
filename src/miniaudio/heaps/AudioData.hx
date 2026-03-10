package miniaudio.heaps;

import haxe.io.Bytes;
import hxd.snd.Data.SampleFormat;

class AudioData extends hxd.snd.Data
{
	final rawData:Bytes;

	public function new(rawData:Bytes, channels:Int, sampleRate:Int, samples:Int, floatFormat:Bool)
	{
		this.rawData = rawData;
		this.channels = channels;
		this.samplingRate = sampleRate;
		this.samples = samples;
		this.sampleFormat = floatFormat ? SampleFormat.F32 : SampleFormat.I16;
	}

	override function decodeBuffer(out:Bytes, outPos:Int, sampleStart:Int, sampleCount:Int):Void
	{
		final bytesPerSample = getBytesPerSample();
		out.blit(outPos, rawData, sampleStart * bytesPerSample, sampleCount * bytesPerSample);
	}
}
