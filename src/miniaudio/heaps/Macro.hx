package miniaudio.heaps;

#if macro
import haxe.macro.Context;
#end

class Macro
{
	public static macro function main()
	{
		#if (macro && hlopus)
		try
		{
			Context.getType('hxd.fmt.opus.Data');
			haxe.macro.Compiler.define('miniaudio_has_hlopus');
		}
		catch (_:Dynamic) {}
		#end

		#if heaps
		hxd.res.Config.addExtension('ogg', 'miniaudio.heaps.AudioBuffer');
		#if miniaudio_has_hlopus
		hxd.res.Config.addExtension('opus', 'miniaudio.heaps.AudioBuffer');
		#end
		#end
		return null;
	}
}
