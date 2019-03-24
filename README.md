ark is an entity component system focused on performance through data-oriented design, compile-time metaprogramming, and effortless parallelization.

Some notable features include:

* Customizable component storage, similar to specs: https://slide-rs.github.io/specs/
  * unique self-defragmenting 'bucket array' component storage provided by default
* Easy parallelization via behind-the-scenes thread pool 
  * Entity level: ```for_each``` --> ```for_each_par``` 
  * System level: ```run_systems_sequential``` --> ```run_systems_parallel``` 
* Concrete notion of a 'System' as a struct with specific typedefs and a 'run' method
* Loose coupling.
  * Systems don't need to know about the entire 'World'.
  * System/World communication handled by a small number of 'handle' types in ark/system.hpp
* No inheritance, no exceptions, no RTTI

### Acknowledgements/Inspiration:

Jonathan Blow for his informative streams and the bucket array idea: https://www.youtube.com/watch?v=COQKyOCAxOQ

Casey Muratori for the Handmade Hero streams and inspiration to think more deeply about these problems: https://handmadehero.org/

Mike Acton for the famous talk you've probably already heard. If not: https://www.youtube.com/watch?v=rX0ItVEVjHc

Malte Skarupke for his great work on optimizing radix sort and open addressing hash tables. Check out his blog: https://probablydance.com/

Other ECS implementations:
    * specs (Rust): https://slide-rs.github.io/specs/
    * entt (C++17): https://github.com/skypjack/entt
