# The Lighthouse Keeper's Lantern

An original demo story for InkQuest, in a public domain setting: you are the new
keeper of a lonely island lighthouse on a storm night, with a ship in distress on
the reef. It is about 30 passages with several endings and uses variables
(courage, and whether you have found the lantern, the key and the signal code),
conditional choices and conditional text.

## Files

- `the-lighthouse-keeper.twee` : the Twine source, in Twee 3 notation with
  Harlowe macros. This is the editable original.
- `the-lighthouse-keeper.iqs` : the compiled story, ready to copy to
  `/inkquest/stories` on an SD card.

## Rebuilding

From the `companion` directory:

```sh
python3 -m inkquest validate ../stories/the-lighthouse-keeper/the-lighthouse-keeper.twee
python3 -m inkquest compile  ../stories/the-lighthouse-keeper/the-lighthouse-keeper.twee \
    -o ../stories/the-lighthouse-keeper/the-lighthouse-keeper.iqs
```

## Cover

An optional cover would live alongside the `.iqs` as
`the-lighthouse-keeper.bmp` (a small 1 bpp or 24 bpp Windows BMP). None is
bundled; the story compiles and runs without one.

## Licence

MIT, the same as the rest of InkQuest. This text is an original work written for
the project.
