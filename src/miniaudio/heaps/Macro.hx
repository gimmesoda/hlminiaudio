package miniaudio.heaps;

#if macro
import haxe.macro.Compiler;
import haxe.macro.Context;
import haxe.macro.Expr;
#end

class Macro
{
	public static macro function main()
	{
		#if macro
		Compiler.addGlobalMetadata('hxd.snd.Manager', '@:build(miniaudio.heaps.Macro.patchManager())', false, true, false);
		#end
		return null;
	}

	#if macro
	public static function patchManager():Array<Field>
	{
		final fields = Context.getBuildFields();
		for (field in fields)
		{
			if (field.name == 'new')
			{
				switch (field.kind)
				{
					case FFun(fn):
						fn.expr = macro
							{
								#if usesys
								driver = new haxe.AudioTypes.SoundDriver();
								#elseif (js && !useal)
								driver = new hxd.snd.webaudio.Driver();
								#else
								driver = new hxd.snd.miniaudio.Driver();
								#end

								STREAM_DURATION = 1000000.;
								BUFFER_QUEUE_LENGTH = 1;

								masterVolume = 1.0;
								hasMasterVolume = driver == null ? true : driver.hasFeature(MasterVolume);
								masterSoundGroup = new SoundGroup("master");
								masterChannelGroup = new ChannelGroup("master");
								listener = new Listener();
								soundBufferMap = new Map();
								soundBufferKeys = [];
								freeStreamBuffers = [];
								effectGC = [];
								soundBufferCount = 0;

								if (driver != null)
								{
									sources = [];
									for (i in 0...MAX_SOURCES)
										sources.push(new Source(driver));
								}

								cachedBytes = haxe.io.Bytes.alloc(4 * 3 * 2);
								resampleBytes = haxe.io.Bytes.alloc(STREAM_BUFFER_SAMPLE_COUNT * 2);
							};
						field.kind = FFun(fn);
					default:
				}
			}

			if (field.name == 'checkTargetFormat')
			{
				switch (field.kind)
				{
					case FFun(fn):
						fn.expr = macro
							{
								if (Std.isOfType(driver, hxd.snd.miniaudio.Driver))
								{
									targetRate = dat.samplingRate;
									targetChannels = forceMono || dat.channels == 1 ? 1 : dat.channels;
									targetFormat = dat.sampleFormat;
									return targetChannels == dat.channels && targetFormat == dat.sampleFormat && targetRate == dat.samplingRate;
								}

								targetRate = dat.samplingRate;
								#if (!usesys && !hlopenal && (!js || useal))
								targetRate = hxd.snd.openal.Emulator.NATIVE_FREQ;
								#end
								targetChannels = forceMono || dat.channels == 1 ? 1 : 2;
								targetFormat = switch (dat.sampleFormat)
								{
									case UI8: UI8;
									case I16: I16;
									#if js
									case F32: F32;
									#else
									case F32: I16;
									#end
								};
								return targetChannels == dat.channels && targetFormat == dat.sampleFormat && targetRate == dat.samplingRate;
							};
						field.kind = FFun(fn);
					default:
				}
			}
		}
		return fields;
	}
	#end
}
